#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include <platform/sios.h>
#include <platform/module.h>
#include <platform/util.h>

/* due to hardware bug R and B are swapped*/
#define SWAP_RB			1

#define LIGHT_DEV_BASE		"/dev/sios_light"
#define MAX_LIGHTS		8
#define COLOR_BUFSIZE 		128
#define INC_N_WRAP_PTR(x)	if (++(x) == COLOR_BUFSIZE) (x) = 0
#define WRITE_MIN_DELAY		20000 /* milliseconds */

MODULE_INIT(light_obj)
SET_MODULE_VERSION(3,0,0)

static char device_base[32] = LIGHT_DEV_BASE;
sios_param_string(device_base, device_base, 32);

int devices = 1;
sios_param(devices, int, devices);

int auto_blink = 0;
sios_param(auto_blink, int, auto_blink);

int auto_blink_speed = 2000;
sios_param(auto_blink_speed, int, auto_blink_speed);

static int dev_light_write(struct sios_source_ctx * ctx, enum sios_event_type action);


struct light_dev 
{
	int num;

	union {
		struct {
			uint16_t rgb[16];
			short direction, steps, step;
			long delay;
		} trans;
		struct color {
			uint16_t rgb;
			long delay; 	/* in milliseconds */
		} color;
	} data;

	struct {
		short intensity;
		long delay;
		enum { NO_FLASH, FLASH } state;
	} flash;

	enum { SINGLE, BLINK, TRANSITION } state;

	uint16_t current;

	struct sios_source_ctx ctx;
	struct sios_source_ctx flash_ctx;
};

#define TYPE_MASK		0xf000
#define COLOR_MASK		0x0fff
#define INTENSITY_MASK		0x0C00
#define DURATION_MASK		0x03FF

#define TYPE_RGB		0x1
#define TYPE_SUB		0x2
#define TYPE_MAIN		0x4
#define TYPE_MAIN_SUB		0x6

#define SET_TYPE(x,w)		((x) = (((x) & ~TYPE_MASK) | ((w) << 12)))
#define SET_INTENSITY(x,w)	((x) = (((x) & ~INTENSITY_MASK) | ((w) << 10)))
#define SET_DURATION(x,w)	((x) = (((x) & ~DURATION_MASK) | (w)))

#ifndef SWAP_RB
#define COLOR_R_BITS		8
#define COLOR_G_BITS		4
#define COLOR_B_BITS		0
#define COLOR_R_MASK		0x0f00
#define COLOR_G_MASK		0x00f0
#define COLOR_B_MASK		0x000f
#else
#define COLOR_R_BITS		0
#define COLOR_G_BITS		4
#define COLOR_B_BITS		8
#define COLOR_R_MASK		0x000f
#define COLOR_G_MASK		0x00f0
#define COLOR_B_MASK		0x0f00
#endif /* SWAP_RB */

#define COLOR_R(x,r)		((x) = (((x) & ~COLOR_R_MASK) | (((r)&0xf) << COLOR_R_BITS)))
#define COLOR_G(x,g)		((x) = (((x) & ~COLOR_G_MASK) | (((g)&0xf) << COLOR_G_BITS)))
#define COLOR_B(x,b)		((x) = (((x) & ~COLOR_B_MASK) | (((b)&0xf) << COLOR_B_BITS)))
#define COLOR_RGB(x,r,g,b)	((x) = (((x) & TYPE_MASK) | (((r)&0xf) << COLOR_R_BITS) \
						   	  | (((g)&0xf) << COLOR_G_BITS) \
						   	  | (((b)&0xf) << COLOR_B_BITS)))
#define	R(x)	(((x) & COLOR_R_MASK) >> COLOR_R_BITS)
#define	G(x)	(((x) & COLOR_G_MASK) >> COLOR_G_BITS)
#define	B(x)	(((x) & COLOR_B_MASK) >> COLOR_B_BITS)

#define MAX_RGB		0xf
#define MAX_INTS	0x3
#define MAX_DURATION	0x400

static struct light_dev light_devs[MAX_LIGHTS];

static inline avance_next_blink_color(struct light_dev * dev)
{
	if (dev->data.trans.step >= dev->data.trans.steps || 
			dev->data.trans.step <= 0) {
		dev->data.trans.direction = -dev->data.trans.direction;
	}
	dev->data.trans.step += dev->data.trans.direction;
}

static void flash(u_short dev, u_short intensity, int duration)
{
	if (intensity > MAX_INTS) intensity = MAX_INTS;
	
	dbg("flash(%d): 0x%x (duration: %d)", dev, intensity, duration);

	light_devs[dev].flash.state = FLASH;
	light_devs[dev].flash.intensity = intensity;
	light_devs[dev].flash.delay = duration * 1000;;
	
	sios_add_source_ctx(&light_devs[dev].flash_ctx);
}

static inline void light_put_color(u_short dev, u_short r, u_short g, u_short b)
{
	uint16_t color = 0;

	if (r > MAX_RGB) r = MAX_RGB;
	if (g > MAX_RGB) g = MAX_RGB;
	if (b > MAX_RGB) b = MAX_RGB;
	
	SET_TYPE(color, TYPE_RGB);
	COLOR_RGB(color, r, g, b);	
	light_devs[dev].state = SINGLE;
	light_devs[dev].data.color.rgb = color;
	light_devs[dev].data.color.delay = WRITE_MIN_DELAY;
	
	sios_add_source_ctx(&light_devs[dev].ctx);
}

static void compute_blink_steps(u_short dev, u_short r1, u_short g1, u_short b1, 
				u_short r2, u_short g2, u_short b2, int duration, 
				short type)
{
	uint16_t r, g, b, steps;
	int16_t dr, dg, db, maxd;
	int16_t sr, sg, sb;
	short i;

	if (r1 > MAX_RGB) r1 = MAX_RGB;
	if (g1 > MAX_RGB) g1 = MAX_RGB;
	if (b1 > MAX_RGB) b1 = MAX_RGB;
	
	if (r2 > MAX_RGB) r2 = MAX_RGB;
	if (g2 > MAX_RGB) g2 = MAX_RGB;
	if (b2 > MAX_RGB) b2 = MAX_RGB;

	dbg("from: dev: %d - 0x%x, 0x%x, 0x%x", dev, r1, g1, b1);
	dbg("to:   dev: %d - 0x%x, 0x%x, 0x%x", dev, r2, g2, b2);

	r = r1; g = g1; b = b1;

	dr = r2 - r1;
	dg = g2 - g1;
	db = b2 - b1;
	
	maxd = (abs(dr) > abs(dg)) 
			? ((abs(dr) > abs(db)) ? abs(dr) : abs(db)) 
			: ((abs(dg) > abs(db)) ? abs(dg) : abs(db));

	if (!maxd)
		return;	

	sr = (dr) ? (maxd + 1) / dr : 0;
	sg = (dg) ? (maxd + 1) / dg : 0;
	sb = (db) ? (maxd + 1) / db : 0;
	
	SET_TYPE(light_devs[dev].data.trans.rgb[0], TYPE_RGB);
	COLOR_RGB(light_devs[dev].data.trans.rgb[0], r1, g1, b1);
	SET_TYPE(light_devs[dev].data.trans.rgb[maxd], TYPE_RGB);
	COLOR_RGB(light_devs[dev].data.trans.rgb[maxd], r2, g2, b2);

	for (i=1;i<maxd;i++) {
		if (sr && !(i % sr)) {
			if (dr > 0) {
				r++;
			} else if (dr < 0) {
				r--;
			}
		}
		if (sg && !(i % sg)) {
			if (dg > 0) {
				g++;
			} else if (dg < 0) {
				g--;
			}
		}
		if (sb && !(i % sb)) {
			if (db > 0) {
				b++;
			} else if (db < 0) {
				b--;
			}
		}

		SET_TYPE(light_devs[dev].data.trans.rgb[i], TYPE_RGB);	
		COLOR_RGB(light_devs[dev].data.trans.rgb[i], r, g, b);	
	}

	light_devs[dev].state = type;
	light_devs[dev].data.trans.steps = maxd;
	light_devs[dev].data.trans.step = 1;
	light_devs[dev].data.trans.direction = 1;
	light_devs[dev].data.trans.delay = (duration / maxd) * 1000;
	
	sios_add_source_ctx(&light_devs[dev].ctx);
}


static void compute_blink_steps_current(u_short dev, u_short r, u_short g, u_short b, 
					int duration, short type)
{
	compute_blink_steps(dev,
			    R(light_devs[dev].current),
			    G(light_devs[dev].current),
			    B(light_devs[dev].current),
			    r, g, b, duration,
			    type);
}

static int color_rgb_handler(const char *path, const char *types, lo_arg **argv, 
			      int argc, lo_message msg, void *user_data)
{
	switch (argc) {
		case 3: 
		{
			u_short i;
			for (i=0; i<devices; i++) 
				light_put_color((u_short)i, (u_short)argv[0]->i, (u_short)argv[1]->i, 
						(u_short)argv[2]->i);
			break;
		}
		case 4:
			light_put_color((u_short)argv[0]->i, (u_short)argv[1]->i, (u_short)argv[2]->i,
					(u_short)argv[3]->i);
			break;
		default:
			warn(MODULE_NAME, "wrong number of arguments: %d", argc);
	}

	return 0;
}

static int color_blink_handler(const char *path, const char *types, lo_arg **argv, 
			      int argc, lo_message msg, void *user_data)
{
	switch (argc) {
		case 4: 
		{
			u_short i;
			for (i=0; i<devices; i++) 
				compute_blink_steps_current((u_short)i, 
							    (u_short)argv[0]->i, (u_short)argv[1]->i, (u_short)argv[2]->i, 
							    argv[3]->i, BLINK);
			break;
		}
		case 5:
			compute_blink_steps_current((u_short)argv[0]->i,
					    	    (u_short)argv[1]->i, (u_short)argv[2]->i, (u_short)argv[3]->i, 
					    	    argv[4]->i, BLINK);
			break;

		case 7: 
		{
			u_short i;
			for (i=0; i<devices; i++) 
				compute_blink_steps((u_short)i, 
						    (u_short)argv[0]->i, (u_short)argv[1]->i, (u_short)argv[2]->i,
						    (u_short)argv[3]->i, (u_short)argv[4]->i, (u_short)argv[5]->i,
						    (u_short)argv[6]->i, BLINK);
			break;
		}
		case 8:
			compute_blink_steps((u_short)argv[0]->i, 
					    (u_short)argv[1]->i, (u_short)argv[2]->i, (u_short)argv[3]->i, 
					    (u_short)argv[4]->i, (u_short)argv[5]->i, (u_short)argv[6]->i,
					    (u_short)argv[7]->i, BLINK);
			break;
		default:
			warn(MODULE_NAME, "wrong number of arguments: %d", argc);
	}

	return 0;
}

static int color_trans_handler(const char *path, const char *types, lo_arg **argv, 
			      int argc, lo_message msg, void *user_data)
{
	switch (argc) {
		case 4: 
		{
			u_short i;
			for (i=0; i<devices; i++) 
				compute_blink_steps_current((u_short)i,
						    	    (u_short)argv[0]->i, (u_short)argv[1]->i, (u_short)argv[2]->i, 
						    	    argv[3]->i, TRANSITION);
			break;
		}
		case 5:
			compute_blink_steps_current((u_short)argv[0]->i,
					    	    (u_short)argv[1]->i, (u_short)argv[2]->i, (u_short)argv[3]->i, 
					    	    argv[4]->i, TRANSITION);
			break;
		case 7: 
		{
			u_short i;
			for (i=0; i<devices; i++) 
				compute_blink_steps((u_short)i, 
						    (u_short)argv[0]->i, (u_short)argv[1]->i, (u_short)argv[2]->i,
						    (u_short)argv[3]->i, (u_short)argv[4]->i, (u_short)argv[5]->i,
						    (u_short)argv[6]->i, TRANSITION);
			break;
		}
		case 8:
			compute_blink_steps((u_short)argv[0]->i, 
					    (u_short)argv[1]->i, (u_short)argv[2]->i, (u_short)argv[3]->i, 
					    (u_short)argv[4]->i, (u_short)argv[5]->i, (u_short)argv[6]->i,
					    (u_short)argv[7]->i, TRANSITION);
			break;

		default:
			warn(MODULE_NAME, "wrong number of arguments: %d", argc);
	}

	return 0;
}

static int color_flash_handler(const char *path, const char *types, lo_arg **argv, 
			      int argc, lo_message msg, void *user_data)
{
	switch (argc) {
		case 2:
		{
			u_short i;
			for (i=0; i<devices; i++) 
				flash(i, argv[0]->i, argv[1]->i);
			break;
		}
		case 3: 
			flash((u_short)argv[0]->i, argv[1]->i, argv[2]->i);
			break;
		default:
			warn(MODULE_NAME, "wrong number of arguments: %d", argc);
	}
	return 0;
}

static int dev_flash_write(struct sios_source_ctx * ctx, enum sios_event_type action) 
{
	struct light_dev * dev;
	uint16_t flash;
	unsigned char data[2];
	int retval;

	//dbg("in flash");
	dev = (struct light_dev*)ctx->priv;

	if (action != SIOS_EVENT_WRITE)
		return 0;
	
	if (dev->flash.state == FLASH) {
		//dbg("FLASH");
		SET_TYPE(flash, TYPE_SUB);
		SET_INTENSITY(flash, dev->flash.intensity);
		dev->flash_ctx.period = dev->flash.delay;
		dev->flash.state = NO_FLASH;
	} else {
		//dbg("NO FLASH");
		SET_TYPE(flash, TYPE_SUB);
		SET_INTENSITY(flash, 0);
		dev->flash_ctx.period = WRITE_MIN_DELAY;
		dev->flash.state = FLASH;
	}

	data[0] = (unsigned char)(flash >> 8);
	data[1] = (unsigned char)(flash & 0x00FF);

	//dbg("flash: 0x%.4x", flash);

	//dbg("pre flash write");
	retval = write(ctx->fd, data, 2);
	if (retval < 0) {
		err(MODULE_NAME, "flash write error '%s%d': %s", device_base, dev->num, strerror(errno));
		return retval;
	}
	//dbg("post flash write");

	return dev->flash.state;
}

static int dev_light_write(struct sios_source_ctx * ctx, enum sios_event_type action) 
{
	struct light_dev * dev;
	unsigned char data[2];
	uint16_t color = 0;
	long delay_next = WRITE_MIN_DELAY;
	int retval, no_repeat = 1;

	//dbg("in light");
	dev = (struct light_dev*)ctx->priv;

	if (action != SIOS_EVENT_WRITE)
		return 0;

	switch (dev->state) {
		case SINGLE:
			//dbg("single");
			color = dev->data.color.rgb;
			delay_next = dev->data.color.delay;
			break;
		case BLINK:
			//dbg("blink");
			color = dev->data.trans.rgb[dev->data.trans.step];
			delay_next = dev->data.trans.delay;
			avance_next_blink_color(dev);
			no_repeat = 0;
			break;
		case TRANSITION:
			//dbg("transition");
			color = dev->data.trans.rgb[dev->data.trans.step];
			delay_next = dev->data.trans.delay;
			no_repeat = (dev->data.trans.step >= dev->data.trans.steps);
			avance_next_blink_color(dev);
			break;
	}
	
	data[0] = (unsigned char)(color >> 8);
	data[1] = (unsigned char)(color & 0x00FF);

	//dbg("pre-write 0x%.2x%.2x", data[0], data[1]);
	retval = write(ctx->fd, data, 2);
	//dbg("post-write");
	if (retval < 0) {
		err(MODULE_NAME, "write error '%s%d': %s", device_base, dev->num, strerror(errno));
		return retval;
	}

	dev->current = color;
	if (delay_next != ctx->period)
		ctx->period = delay_next;

	//dbg("done");
	return no_repeat;
}

static int open_light_dev(const char * dev)
{
	int fd = open(dev, O_WRONLY | O_NONBLOCK);
	if (fd < 0) {
		err(MODULE_NAME, "error opening '%s': %s", dev, strerror(errno));
		return -1;
	}
	return fd;
}

static void close_light_dev(int fd)
{
	close(fd);
}

static int init_devices(int numdevs) 
{
	int i, retval = 0;

	for (i=0; i<numdevs; i++) {
		char name[40];
		int fd; 

		snprintf(name, 40, "%s%d", device_base, i);
		info(MODULE_NAME, "openening dev: %s", name);

		fd = open_light_dev(name);	
		if (fd < 0) {
			light_devs[i].ctx.fd = -1;
			retval++;
			continue;
		}

		light_devs[i].num = i;

		light_devs[i].ctx.self = THIS_MODULE;
		light_devs[i].ctx.type = SIOS_POLL_WRITE;
		light_devs[i].ctx.priority = SIOS_PRIORITY_HIGH;
		light_devs[i].ctx.handler = dev_light_write;
		light_devs[i].ctx.fd = fd;
		light_devs[i].ctx.priv = &light_devs[i];
		light_devs[i].ctx.period = WRITE_MIN_DELAY;

		light_devs[i].flash_ctx.self = THIS_MODULE;
		light_devs[i].flash_ctx.type = SIOS_POLL_WRITE;
		light_devs[i].flash_ctx.priority = SIOS_PRIORITY_MAX;
		light_devs[i].flash_ctx.handler = dev_flash_write;
		light_devs[i].flash_ctx.fd = fd;
		light_devs[i].flash_ctx.priv = &light_devs[i];
		light_devs[i].flash_ctx.period = WRITE_MIN_DELAY;
	}

	return retval;
}

static struct sios_method_desc osc_methods[] = {
	METHOD_DESC_INITIALIZER("rgb", "", NULL, color_rgb_handler, "set rgb color"),
	METHOD_DESC_INITIALIZER("blink", "", NULL, color_blink_handler, "blink colors"),
	METHOD_DESC_INITIALIZER("trans", "", NULL, color_trans_handler, "smooth fading to color"),
	METHOD_DESC_INITIALIZER("flash", "", NULL, color_flash_handler, "flash"),
};


int light_init(void)
{
	int retval = 0;
	int fd = 0;

	retval = init_devices(devices);
	if (retval) {
		err(MODULE_NAME, "error opening %d light devices",  retval);
		return -retval;
	}
		
	retval = sios_object_register(THIS_MODULE, THIS_CLASS);
	if (retval) {
		err(MODULE_NAME, "error registering light object");
		return retval;
	}

	sios_osc_add_method_descs(osc_methods, METHOD_DESCRIPTORS(osc_methods));

	if (retval < 0) {
		sios_object_deregister(THIS_MODULE);
		return -1;
	}

	if (auto_blink) {
		int dev;
		for (dev=0;dev<devices;dev++) {
			compute_blink_steps(dev, 0, 0, 1, 0, 0, 15, auto_blink_speed, BLINK);
		}
	}

	return 0;
}


void light_exit(void)
{
	int i;
	
	for (i=0; i<devices; i++) {
		sios_del_source_ctx(&light_devs[i].ctx);
		close_light_dev(light_devs[i].ctx.fd);
	}
	sios_object_deregister(THIS_MODULE);
}

module_init(light_init)
module_exit(light_exit)
