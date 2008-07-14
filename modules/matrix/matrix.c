#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <platform/sios.h>
#include <platform/module.h>
#include <platform/util.h>

#define MATRIX_DEV	"/dev/sios_matrix"
#define MAX_CELLS	64
#define MAX_ROWS	8
#define MAX_COLS	8
#define BUFSIZE		MAX_CELLS * 2

MODULE_INIT(matrix_obj)
SET_MODULE_VERSION(1, 0, 2)

static char device[36] = MATRIX_DEV;
static int rows = MAX_ROWS;
static int cols = MAX_COLS;
static unsigned char buf[BUFSIZE];

sios_param(rows, int, rows);
sios_param(cols, int, cols);
sios_param_string(device, device, 36);

static char * matrix_path[] = { "/sios/sensors/matrix/data" };

static uint16_t matrix_data[MAX_CELLS];

LIST_HEAD(listen_list);
static pthread_mutex_t listener_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t matrix_reader_loop_thread;
static pthread_mutex_t halt_lock = PTHREAD_MUTEX_INITIALIZER;
static int halt = 0;

static int add_listener(lo_address addr)
{
	struct listener * l; 

	if (!addr)
		return -1;

	pthread_mutex_lock(&listener_lock);
	list_for_each_entry(l, &listen_list, listener) {
		if (!strcmp(lo_address_get_hostname(addr), lo_address_get_hostname(l->address)) &&
				!strcmp(lo_address_get_port(addr), lo_address_get_port(l->address))) {
			info(MODULE_NAME, "matrix: %s:%s already a listener", 
					  lo_address_get_hostname(addr),
					  lo_address_get_port(addr));
			pthread_mutex_unlock(&listener_lock);
			return -1;
		}
	}

	l = (struct listener*)malloc(sizeof(struct listener));
	if (!l) {
		pthread_mutex_unlock(&listener_lock);
		return -1;
	}

	INIT_LIST_HEAD(&l->listener);
	l->address = addr;
	list_add(&l->listener, &listen_list);

	pthread_mutex_unlock(&listener_lock);

	info(MODULE_NAME, "sending matrix data to: %s:%s", lo_address_get_hostname(addr), 
	 						     lo_address_get_port(addr));
	
	return 0;
}

static void del_listener(lo_address addr)
{
	struct listener * l; 
	int found = 0;

	if (!addr) return;

	pthread_mutex_lock(&listener_lock);
	list_for_each_entry(l, &listen_list, listener) {
		if (!strcmp(lo_address_get_hostname(addr), lo_address_get_hostname(l->address)) &&
				!strcmp(lo_address_get_port(addr), lo_address_get_port(l->address))) {
			found = 1;
			break;
		}
	}

	if (found) {
		info(MODULE_NAME, "stop sending matrix data to: %s:%s", lo_address_get_hostname(l->address), 
							       		  lo_address_get_port(l->address));
		list_del(&l->listener);
		lo_address_free(l->address);
		free(l);
	}

	pthread_mutex_unlock(&listener_lock);
}

static int add_matrix_listen_source_handler(const char *path, const char *types, lo_arg **argv, 
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

	if (add_listener(addr)) {
		lo_address_free(addr);
		return -1;
	}

	return 0;
}

static int del_matrix_listen_source_handler(const char *path, const char *types, lo_arg **argv, 
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

	del_listener(addr);
	lo_address_free(addr);
	return 0;
}



static int dev_matrix_read(struct sios_source_ctx * ctx, enum sios_event_type action) 
{
	struct listener * l;
	static int ptr = 0;
	int bytes, i,j;

	if (action != SIOS_EVENT_READ)
		return 0;
	
	bytes = read(ctx->fd, buf+ptr, BUFSIZE);
	if (bytes < 0) {
		err(MODULE_NAME, "matrix read error (%d): '%s'", errno, strerror(errno));
		return 0;
	} else if (bytes < BUFSIZE) {
		ptr += bytes;
	} else {
		pthread_mutex_lock(&listener_lock);

		if (!list_empty(&listen_list)) {
#if 0
			int i;
			memcpy(matrix_data, buf, BUFSIZE);
			lo_message msg = lo_message_new();
			for (i=0;i<rows;i++) 
				for (j=0;j<cols;j++) 
					lo_message_add_int32(msg, ((unsigned int)matrix_data[i*MAX_COLS + j]) >> 4);
			list_for_each_entry(l, &listen_list, listener) 
				lo_send_message(l->address, matrix_path[0], msg);
			lo_message_free(msg);
#else
			int v[64], i;
			for (i=0;i<128;i+=2) {
				v[i/2] = ((buf[i] << 8) | (buf[i+1] & 0x0ff)) & 0x0fff;
			}
/*
			sios_osc_dispatch_all(  matrix_path[0], 
					        "iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii",
						v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],
						v[8],v[9],v[10],v[11],v[12],v[13],v[14],v[15],
						v[16],v[17],v[18],v[19],v[20],v[21],v[22],v[23],
						v[24],v[25],v[26],v[27],v[28],v[29],v[30],v[31],
						v[32],v[33],v[34],v[35],v[36],v[37],v[38],v[39],
						v[40],v[41],v[42],v[43],v[44],v[45],v[46],v[47],
						v[48],v[49],v[50],v[51],v[52],v[53],v[54],v[55],
						v[56],v[57],v[58],v[59],v[60],v[61],v[62],v[63]);

*/
			if (rows == 8 && cols == 8) {
				list_for_each_entry(l, &listen_list, listener) {
					lo_send(l->address, matrix_path[0], 
						"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii",
						v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],
						v[8],v[9],v[10],v[11],v[12],v[13],v[14],v[15],
						v[16],v[17],v[18],v[19],v[20],v[21],v[22],v[23],
						v[24],v[25],v[26],v[27],v[28],v[29],v[30],v[31],
						v[32],v[33],v[34],v[35],v[36],v[37],v[38],v[39],
						v[40],v[41],v[42],v[43],v[44],v[45],v[46],v[47],
						v[48],v[49],v[50],v[51],v[52],v[53],v[54],v[55],
						v[56],v[57],v[58],v[59],v[60],v[61],v[62],v[63]);
				}
			} else if (rows == 4 && cols == 16) {
				list_for_each_entry(l, &listen_list, listener) {
					lo_send(l->address, matrix_path[0], 
						"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii",
						v[63],v[55],v[47],v[39],v[31],v[23],v[15],v[7], v[59],v[51],v[43],v[35],v[27],v[19],v[11],v[3],
						v[62],v[54],v[48],v[38],v[30],v[22],v[14],v[6], v[58],v[50],v[42],v[34],v[26],v[18],v[10],v[2],
						v[61],v[53],v[47],v[37],v[29],v[21],v[13],v[5], v[57],v[49],v[41],v[33],v[25],v[17],v[9], v[1],
						v[60],v[52],v[46],v[36],v[28],v[19],v[12],v[4], v[56],v[48],v[40],v[32],v[24],v[16],v[8], v[0]);
				}
			}
#endif
		}

		pthread_mutex_unlock(&listener_lock);
		
		bzero(buf, BUFSIZE);
		ptr = 0;
	}
	return 0;
}

static void * matrix_reader_loop(void * arg)
{
	struct sios_source_ctx * ctx = (struct sios_source_ctx*)arg;
	fd_set read_set;
	struct timeval wait;
	int n;

	info("Core", "matrix reader loop started");
	while (1) {
		pthread_mutex_lock(&halt_lock);
		if (halt) {
			pthread_mutex_unlock(&halt_lock);
			return NULL;
		}
		pthread_mutex_unlock(&halt_lock);

		FD_ZERO(&read_set);
		FD_SET(ctx->fd, &read_set);

		wait.tv_sec = 0;
		wait.tv_usec = 50000;
		n = select(ctx->fd + 1, &read_set, NULL, NULL, &wait);
		if (n < 0) {
			if (errno != EINTR)
				continue;
		} else {
			if (FD_ISSET(ctx->fd, &read_set)) {
				dev_matrix_read(ctx, SIOS_EVENT_READ);
			}
		}

	}
}

static int open_matrix_dev(const char * dev)
{
	int fd = open(dev, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		err(MODULE_NAME, "error opening '%s': %s", dev, strerror(errno));
		return -1;
	}
	return fd;
}

static void close_matrix_dev(int fd)
{
	close(fd);
}

static struct sios_source_ctx dev_matrix_src = {
	.type = SIOS_POLL_READ,
	.priority = SIOS_PRIORITY_DEFAULT,
	.handler = dev_matrix_read,
};

struct sios_method_desc osc_methods[] = {
	METHOD_DESC_INITIALIZER("listen", "", NULL, add_matrix_listen_source_handler, "start data transfer"),
	METHOD_DESC_INITIALIZER("silence", "", NULL, del_matrix_listen_source_handler, "stop data transfer"),
};

int matrix_init(void)
{
	int retval = 0;
	int fd = 0;

	retval = sios_object_register(THIS_MODULE, THIS_CLASS);
	if (retval) {
		err(MODULE_NAME, "failed registering matrix object");
		return retval;
	}


	retval = open_matrix_dev(device);
	if (retval < 0) {
		sios_object_deregister(THIS_MODULE);
		err(MODULE_NAME, "error opening matrix device: %s",  MATRIX_DEV);
		return -1;
	}

	fd = retval;

//	sios_object_can_have_listeners(THIS_MODULE);

	retval = sios_osc_add_method_descs(osc_methods, METHOD_DESCRIPTORS(osc_methods));

	dev_matrix_src.self = THIS_MODULE;
	dev_matrix_src.fd = fd;

//	sios_add_source_ctx(&dev_matrix_src);

	retval = pthread_create(&matrix_reader_loop_thread, NULL, matrix_reader_loop, &dev_matrix_src);
	if (retval) {
		err("Core", "failed pthread_create");
		return retval;
	}


	return 0;
}


void matrix_exit(void)
{
	pthread_mutex_lock(&halt_lock);
	halt = 1;
	pthread_mutex_unlock(&halt_lock);

	dbg("joining main reader thread");
	pthread_join(matrix_reader_loop_thread, NULL);
	dbg("joining main reader thread done");

	close_matrix_dev(dev_matrix_src.fd);
	//sios_del_source_ctx(&dev_matrix_src);
	sios_object_deregister(THIS_MODULE);
}

module_init(matrix_init)
module_exit(matrix_exit)

