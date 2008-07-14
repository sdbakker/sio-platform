#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <platform/sios.h>
#include <platform/module.h>

#ifdef SIOS_FIXED_POINT
#include <platform/fixed.h>
#endif

#define ACCMAG_SYS_BASE		"/sys/class/sensors/sios_accmag"
#define ACCMAG_MAG_PULSE	"mag_pulse"
#define ACCMAG_DEV_BASE		"/dev/sios_accmag"

MODULE_INIT(accmag_obj)
SET_MODULE_VERSION(2,0,1)

static char device_base[32] = ACCMAG_DEV_BASE;
sios_param_string(device_base, device_base, 32);

int devices = 1;
sios_param(devices, int, devices);

int calibration_samples = 3;
sios_param(calibration_samples, int, calibration_samples);

int verbose = 0;
sios_param(verbose, int, verbose);

#define ACCMAG_DATA_SIZE	6

#define AM	0
#define MM	1

struct accmag_data {
	int16_t x, y, z;
} __attribute__((packed));

struct accmag_dev {
	char mag_pulse_path[64];
	int mag_pulse_state;
	struct {
		enum { C_no, C_norm, C_inv } state;
		int samples, sample;
		struct accmag_data *norm, *inv;
		struct accmag_data offset;
		int first;
	} c_data;
	int num;
	int type;
};

#define ACCMAG_DEVS(_devs) (signed int)((_devs) ? (sizeof(*(_devs)) / sizeof(struct accmag_dev)) : 0)
static struct accmag_dev * devs = NULL;
#define ACCMAG_SOURCES(_ctxs) (signed int)((_ctxs) ? (sizeof(*(_ctxs)) / sizeof(struct sios_source_ctx)) : 0)
static struct sios_source_ctx * ctxs = NULL;

LIST_HEAD(am_listen_list);
LIST_HEAD(mm_listen_list);
static pthread_mutex_t am_listener_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mm_listener_lock = PTHREAD_MUTEX_INITIALIZER;

static char * accmag_path[] = { "/sios/sensors/accmag/acc/data", "/sios/sensors/accmag/mag/data" };

static int add_listener(lo_address addr, int type)
{
	struct listener * l; 
	struct list_head * ll;
	pthread_mutex_t * ll_lock;

	if (!addr)
		return -1;

	ll = (type) ? &mm_listen_list : &am_listen_list;
	ll_lock = (type) ? &mm_listener_lock : &am_listener_lock;

	pthread_mutex_lock(ll_lock);

	list_for_each_entry(l, ll, listener) {
		if (!strcmp(lo_address_get_hostname(addr), lo_address_get_hostname(l->address)) &&
				!strcmp(lo_address_get_port(addr), lo_address_get_port(l->address))) {
			info(MODULE_NAME, "accmag: %s:%s already a listener", 
					lo_address_get_hostname(addr),
					lo_address_get_port(addr));
			pthread_mutex_unlock(ll_lock);
			return -1;
		}
	}

	l = (struct listener*)malloc(sizeof(struct listener));
	if (!l) {
		pthread_mutex_unlock(ll_lock);
		return -1;
	}

	INIT_LIST_HEAD(&l->listener);
	l->address = addr;
	list_add(&l->listener, ll);

	pthread_mutex_unlock(ll_lock);

	info(MODULE_NAME, "sending %s data to: %s:%s", (type) ? "magnetometer" : "accelerometer", 
							 lo_address_get_hostname(addr), 
						   	 lo_address_get_port(addr));
	return 0;
}

static void del_listener(lo_address addr, int type)
{
	struct listener * l; 
	pthread_mutex_t * ll_lock;
	int found = 0;

	if (!addr) return;

	ll_lock = (type) ? &mm_listener_lock : &am_listener_lock;

	pthread_mutex_lock(ll_lock);

	list_for_each_entry(l, (type) ? &mm_listen_list : &am_listen_list, listener) {
		if (!strcmp(lo_address_get_hostname(addr), lo_address_get_hostname(l->address)) &&
				!strcmp(lo_address_get_port(addr), lo_address_get_port(l->address))) {
			found = 1;
			break;
		}
	}

	if (found) {
		info(MODULE_NAME, "stop sending %s data to: %s:%s", (type) ? "magnetometer" : "accelerometer", 
								      lo_address_get_hostname(l->address), 
								      lo_address_get_port(l->address));
		list_del(&l->listener);
		lo_address_free(l->address);
		free(l);
	}

	pthread_mutex_unlock(ll_lock);
}

static int del_acc_listen_source_handler(const char *path, const char *types, lo_arg **argv, 
			      int argc, lo_message msg, void *user_data)
{
	lo_address addr, t = lo_message_get_source(msg);

	if (argc < 2) {
		info(MODULE_NAME, "delling from source");
		addr = lo_address_new(lo_address_get_hostname(t), lo_address_get_port(t));
	} else {
		info(MODULE_NAME, "delling from args %s", types);
		if (types[1] == 'i') {
			char port[6];
			snprintf(port, 6, "%d", argv[1]->i);
			addr = lo_address_new(&argv[0]->s, port);
		} else {
			addr = lo_address_new(&argv[0]->s, &argv[1]->s);
		}
	}

	del_listener(addr, AM);
	lo_address_free(addr);
	return 0;
}

static int del_mag_listen_source_handler(const char *path, const char *types, lo_arg **argv, 
			      int argc, lo_message msg, void *user_data)
{
	lo_address addr, t = lo_message_get_source(msg);

	if (argc < 2) {
		info(MODULE_NAME, "delling from source");
		addr = lo_address_new(lo_address_get_hostname(t), lo_address_get_port(t));
	} else {
		info(MODULE_NAME, "delling from args %s", types);
		if (types[1] == 'i') {
			char port[6];
			snprintf(port, 6, "%d", argv[1]->i);
			addr = lo_address_new(&argv[0]->s, port);
		} else {
			addr = lo_address_new(&argv[0]->s, &argv[1]->s);
		}
	}

	del_listener(addr, MM);
	lo_address_free(addr);
	return 0;
}

static int add_acc_listen_source_handler(const char *path, const char *types, lo_arg **argv, 
			      int argc, lo_message msg, void *user_data)
{
	lo_address addr, t = lo_message_get_source(msg);

	if (argc < 2) {
		info(MODULE_NAME, "adding from source");
		addr = lo_address_new(lo_address_get_hostname(t), lo_address_get_port(t));
	} else {
		info(MODULE_NAME, "adding from args %s", types);
		if (types[1] == 'i') {
			char port[6];
			snprintf(port, 6, "%d", argv[1]->i);
			addr = lo_address_new(&argv[0]->s, port);
		} else {
			addr = lo_address_new(&argv[0]->s, &argv[1]->s);
		}
	}

	if (add_listener(addr, AM)) {
		lo_address_free(addr);
		return -1;
	}

	return 0;
}

static int add_mag_listen_source_handler(const char *path, const char *types, lo_arg **argv, 
			      int argc, lo_message msg, void *user_data)
{
	lo_address addr, t = lo_message_get_source(msg);

	if (argc < 2) {
		info(MODULE_NAME, "adding from source");
		addr = lo_address_new(lo_address_get_hostname(t), lo_address_get_port(t));
	} else {
		info(MODULE_NAME, "adding from args %s", types);
		if (types[1] == 'i') {
			char port[6];
			snprintf(port, 6, "%d", argv[1]->i);
			addr = lo_address_new(&argv[0]->s, port);
		} else {
			addr = lo_address_new(&argv[0]->s, &argv[1]->s);
		}
	}

	if (add_listener(addr, MM)) {
		lo_address_free(addr);
		return -1;
	}

	return 0;
}

static int dev_accmag_calibrate_mag(int devnum, int samples);

static int mag_calibrate_handler (const char *path, const char *types, lo_arg **argv, 
			      int argc, lo_message msg, void *user_data)
{
	int samples, dev;
	if (argc < 1)
		return -1;
	else if (argc > 1)
		samples = argv[1]->i;
	else 
		samples = calibration_samples;
	dev = argv[0]->i;

	if (dev >= 0 && samples > 0) {
		info(MODULE_NAME, "calibration request %d, %d", dev, samples);
		dev_accmag_calibrate_mag(dev, samples);
	}
}

static int dev_accmag_toggle_magpulse(int devnum)
{
	struct accmag_dev * dev = &devs[devnum];
	int fd, n;
	char toggle[3];

	dev->mag_pulse_state = !dev->mag_pulse_state;

	info(MODULE_NAME, "accmag toggling mag_pulse %s (%d, %d)", dev->mag_pulse_path, devnum, dev->mag_pulse_state);
	fd = open(dev->mag_pulse_path, O_WRONLY | O_NONBLOCK);
	if (fd < 0) {
		err(MODULE_NAME, "error opening 'dev': %s", strerror(errno));
		return -1;
	}
	
	n = snprintf(toggle, 3, "%d", dev->mag_pulse_state);
	write(fd, toggle, 3);
	close(fd);

	if (n < 3)
		return -1;
	return 0;
}

static void dev_mag_calc_offset(struct accmag_dev *dev) 
{
	struct {signed int x; signed int y; signed int z;} avg_n = {0, 0, 0}, avg_i = {0, 0, 0};
	struct accmag_data act = {0, 0, 0};	
	int i;
#ifdef SIOS_FIXED_POINT
	mm_fixed_t tmp_x, tmp_y, tmp_z;
#endif

	info(MODULE_NAME, "accmag calculating offsets");
	
	for (i=0;i<dev->c_data.samples;i++) {
		info(MODULE_NAME, "norm: [0x%x, 0x%x, 0x%x], inv: [0x%x, 0x%x, 0x%x]", 
				dev->c_data.norm[i].x, dev->c_data.norm[i].y, dev->c_data.norm[i].z,
				dev->c_data.inv[i].x, dev->c_data.inv[i].y, dev->c_data.inv[i].z);
		avg_n.x += dev->c_data.norm[i].x; avg_n.y += dev->c_data.norm[i].y; avg_n.z += dev->c_data.norm[i].z;
		avg_i.x += dev->c_data.inv[i].x; avg_i.y += dev->c_data.inv[i].y; avg_i.z += dev->c_data.inv[i].z;
		info(MODULE_NAME, "avg norm: [0x%x, 0x%x, 0x%x], inv: [0x%x, 0x%x, 0x%x]", 
				avg_n.x, avg_n.y, avg_n.z,
				avg_i.x, avg_i.y, avg_i.z);

	}
	avg_n.x /= dev->c_data.samples; avg_n.y /= dev->c_data.samples; avg_n.z /= dev->c_data.samples;
	avg_i.x /= dev->c_data.samples; avg_i.y /= dev->c_data.samples; avg_i.z /= dev->c_data.samples;

#ifdef SIOS_FIXED_POINT
	tmp_x = fdivff(itofix(avg_n.x - avg_i.x), itofix(2));
	tmp_y = fdivff(itofix(avg_n.y - avg_i.y), itofix(2));
	tmp_z = fdivff(itofix(avg_n.z - avg_i.z), itofix(2));

	act.x = fixtoi(tmp_x);
	act.y = fixtoi(tmp_y);
	act.z = fixtoi(tmp_z);
#else
	act.x = (int)((float)(avg_n.x - avg_i.x) / 2.0);
	act.y = (int)((float)(avg_n.y - avg_i.y) / 2.0);
	act.z = (int)((float)(avg_n.z - avg_i.z) / 2.0);
#endif

	dev->c_data.offset.x = act.x - avg_n.x;
	dev->c_data.offset.y = act.y - avg_n.y;
	dev->c_data.offset.z = act.z - avg_n.z;
	info(MODULE_NAME, "accmag have offsets (%d, %d, %d)", 
			dev->c_data.offset.x,
			dev->c_data.offset.y,
			dev->c_data.offset.z);
}

static int dev_accmag_calibrate_mag(int devnum, int samples)
{
	struct accmag_dev * dev = &devs[devnum+1];

	info(MODULE_NAME, "initialize calibration: %d, %d", devnum, samples );
	if (dev->c_data.state != C_no) {
		warn(MODULE_NAME, "accmag already in callibration sequence");
		return -1;
	}
	
	if (dev->c_data.samples == samples) {
		if (dev->c_data.norm != NULL && dev->c_data.inv != NULL) {
			/* reset callibration samples */
			memset(dev->c_data.norm, 0x0, sizeof(*dev->c_data.norm) * samples);
			memset(dev->c_data.inv, 0x0, sizeof(*dev->c_data.inv) * samples);
		} else {
			/* resize callibration sample space */
			free(dev->c_data.norm);
			free(dev->c_data.inv);
			dev->c_data.norm = (struct accmag_data*)malloc(sizeof(struct accmag_data)*samples);
			dev->c_data.inv = (struct accmag_data*)malloc(sizeof(struct accmag_data)*samples);

			
		}
	} else {
		free(dev->c_data.norm);
		free(dev->c_data.inv);
		dev->c_data.norm = (struct accmag_data*)malloc(sizeof(struct accmag_data)*samples);
		dev->c_data.inv = (struct accmag_data*)malloc(sizeof(struct accmag_data)*samples);
	}

	if (!dev->c_data.norm || !dev->c_data.inv) {
		info(MODULE_NAME, "accmag could not allocate sample space");
		free(dev->c_data.norm);
		free(dev->c_data.inv);
		return -1;
	}

	dev->c_data.state = C_norm;
	dev->c_data.samples = samples;
	dev->c_data.sample = 0;
	dev->c_data.offset.x = 0;
	dev->c_data.offset.y = 0;
	dev->c_data.offset.z = 0;

	return 0;
}

static int dev_accmag_read(struct sios_source_ctx * ctx, enum sios_event_type action) 
{
	struct listener * l;
	struct list_head * ll;
	struct accmag_dev * dev = (struct accmag_dev*)ctx->priv;
	struct accmag_data data;
	int bytes;

	if (action != SIOS_EVENT_READ)
		return 0;
	
	bytes = read(ctx->fd, (char*)&data, ACCMAG_DATA_SIZE);
	if (bytes < ACCMAG_DATA_SIZE) {
		if (bytes < 0) 
			err(MODULE_NAME, "accmag read error (%d): %s", bytes, strerror(bytes));
		else 
			warn(MODULE_NAME, "accmag read only %d bytes, ignoring", bytes);
		return 0;
	} else {
		if (dev->type && dev->c_data.state != C_no) {
			info(MODULE_NAME, "accmag capturing calibration samples (%d,%d)", dev->type, dev->c_data.state);
			if (dev->c_data.state == C_norm) {
				info(MODULE_NAME, "getting normal sample %d", dev->c_data.sample + 1);
				memcpy(&dev->c_data.norm[dev->c_data.sample], &data, sizeof(data));
				if (++dev->c_data.sample == dev->c_data.samples) {
					info(MODULE_NAME, "accmag captured enough norm samples");
					dev->c_data.state = C_inv;
					dev->c_data.sample = 0;
					dev->c_data.first = 1;
					dev_accmag_toggle_magpulse(dev->num);
				}
			} else if (dev->c_data.state == C_inv) {
				if (dev->c_data.first) {
					info(MODULE_NAME, "accmag skipping first inv sample");
					dev->c_data.first = 0;
				} else {
					info(MODULE_NAME, "getting invert sample %d", dev->c_data.sample + 1);
					memcpy(&dev->c_data.inv[dev->c_data.sample], &data, sizeof(data));
					if (++dev->c_data.sample == dev->c_data.samples) {
						info(MODULE_NAME, "accmag captured enough inv samples");
						dev->c_data.state = C_no;
						dev_accmag_toggle_magpulse(dev->num);
						dev_mag_calc_offset(dev);
					}
				}
			}
		} else {
			pthread_mutex_t * ll_lock;

			if (dev->type) {
				data.x += dev->c_data.offset.x;
				data.y += dev->c_data.offset.y;
				data.z += dev->c_data.offset.z;
			}

			ll = (dev->type) ? &mm_listen_list : &am_listen_list;
			ll_lock = (dev->type) ? &mm_listener_lock : &am_listener_lock;

			pthread_mutex_lock(ll_lock);

			if (!list_empty(ll)) {
				lo_message msg = lo_message_new();
				lo_message_add_int32(msg, dev->num);
				lo_message_add_int32(msg, (int)data.x);
				lo_message_add_int32(msg, (int)data.y);
				lo_message_add_int32(msg, (int)data.z);
				list_for_each_entry(l, ll, listener) {
					sios_osc_dispatch_msg(l->address, 
							      accmag_path[dev->type],
							      msg);
				}
				lo_message_free(msg);
				if (verbose)
					info(MODULE_NAME, "%s data: %d\t%d\t%d", 
							  (dev->type) ? "mag" : "acc", 
							  (int)data.x, (int)data.y, (int)data.z);
			}

			pthread_mutex_unlock(ll_lock);

		}
	}
	return 0;
}

static int open_accmag_dev(const char * dev)
{
	int fd = open(dev, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		err(MODULE_NAME, "error opening %s: %s", dev, strerror(errno));
		return -1;
	}
	return fd;
}

static int init_devices(int num) 
{
	int i, retval = 0;
	
	if (num <= 0) return -1;

	ctxs = (struct sios_source_ctx*)malloc(sizeof(struct sios_source_ctx) * num * 2);
	if (ctxs == NULL) return -1;

	devs = (struct accmag_dev*)malloc(sizeof(struct accmag_dev) * num * 2);
	if (devs == NULL) return -1;
	
	for (i=0;i<num*2;i++) {
		char name[40];
		int num = i/2;
		int type = i%2;
		int fd;

		snprintf(name, 40, "%s%d%c", device_base, num, (type) ? 'm' : 'a' );
		info(MODULE_NAME, "openening %s dev: %s", (type) ? "mag" : "acc", name);
		
		fd = open_accmag_dev(name);	
		if (fd < 0) {
			ctxs[i].fd = -1;
			retval++;
			continue;
		}

		snprintf(devs[i].mag_pulse_path, 64, "%s%dm/%s", ACCMAG_SYS_BASE, num, ACCMAG_MAG_PULSE);
		devs[i].mag_pulse_state = 0;
		devs[i].num = num;
		devs[i].type = type;
		devs[i].c_data.state = C_no;
		devs[i].c_data.samples = 0;
		devs[i].c_data.sample = 0;
		devs[i].c_data.offset.x = 0;
		devs[i].c_data.offset.y = 0;
		devs[i].c_data.offset.z = 0;
	
		ctxs[i].self = THIS_MODULE;
		ctxs[i].type = SIOS_POLL_READ;
		ctxs[i].priority = SIOS_PRIORITY_DEFAULT;
		ctxs[i].handler = dev_accmag_read;
		ctxs[i].fd = fd;
		ctxs[i].priv = &devs[i];
	}

	return retval;
}

/* FIXME
 * Rewrite accmag driver in a better way ;0
 */
struct sios_method_desc osc_methods[] = {
	METHOD_DESC_INITIALIZER("acc_listen", "acc/listen", NULL, add_acc_listen_source_handler, "start data transfer"),
	METHOD_DESC_INITIALIZER("mag_listen", "mag/listen", NULL, add_mag_listen_source_handler, "start data transfer"),
	METHOD_DESC_INITIALIZER("acc_silence", "acc/silence", NULL, del_acc_listen_source_handler, "stop data transfer"),
	METHOD_DESC_INITIALIZER("mag_silence", "mag/silence", NULL, del_mag_listen_source_handler, "stop data transfer"),
	METHOD_DESC_INITIALIZER("mag_calibrate", "mag/calibrate", NULL, mag_calibrate_handler, "calibrate magnetometer"),
};


int accmag_init(void)
{
	int i, retval = 0;

	retval = init_devices(devices);
	if (retval > 0) {
		err(MODULE_NAME, "error opening %d acc/mag devices",  retval);
		return -retval;
	} else if (retval < 0) {
		err(MODULE_NAME, "out of memory while opening devices");
		return retval;
	}
	
	retval = sios_object_register(THIS_MODULE, THIS_CLASS);
	if (retval) {
		err(MODULE_NAME, "error registering accmag object");
		return retval;
	}
	
	info(MODULE_NAME, "have sources: %d", ACCMAG_SOURCES(ctxs));
	for (i=0;i<devices*2;i++) {
		if (ctxs[i].fd > 0)
			if (sios_add_source_ctx(&ctxs[i])) 
				retval++;
	}

	if (retval) {
		err(MODULE_NAME, "error adding %d acc/mag sources", retval);
		sios_object_deregister(THIS_MODULE);
		return -1;
	}
	retval = sios_osc_add_method_descs(osc_methods, METHOD_DESCRIPTORS(osc_methods));

	return retval;
}


void accmag_exit(void)
{
	int i;
	for (i=0;i<ACCMAG_SOURCES(ctxs);i++) {
		close(ctxs[i].fd);
		sios_del_source_ctx(&ctxs[i]);
	}
	sios_object_deregister(THIS_MODULE);
}

module_init(accmag_init)
module_exit(accmag_exit)
