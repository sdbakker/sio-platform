#include <sys/types.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>


#define MAG_PATH "/dev/sios_accmag0a"
#define ACC_PATH "/dev/sios_accmag0m"
#define MAT_PATH "/dev/sios_matrix"

#include "timediff.h"
#include "util.h"

static volatile short halt = 0;

struct accmag_data {
	int16_t x, y, z;
} __attribute__((packed));


static void handle_sigint(int sigraised)
{
	halt = 1;
}

int main(int argc, char * argv[])
{
	struct timeval wait;
	static fd_set read_set;
	suseconds_t max_wait = 500;
	int ac_fd, ma_fd, mt_fd, max_fd;
	int bytes;

	struct accmag_data acc_data, mag_data;;
	unsigned char mat_data[128];

	long ac_dt = 0, ma_dt = 0,  mt_dt = 0;
	int ac_cnt = 0, ma_cnt = 0, mt_cnt = 0;

	int blocking = 0;
	
	signal(SIGINT, handle_sigint);
	signal(SIGQUIT, handle_sigint);

	while(1) {
		static int c;
		int option_index = 0;
		static struct option long_options[] = {
			{"block", 0, 0, 'b'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "b", long_options, &option_index);
		if (c<0)
			break;
		switch(c) {
			case 'b':
				blocking = 1;
			case 'h':
			case '?':
				//usage(argv[0]);
			default:
				break;
		}
	}

	if (blocking) {
		printf("blocking\n");
		ac_fd = open(ACC_PATH, O_RDONLY);
		ma_fd = open(MAG_PATH, O_RDONLY);
		mt_fd = open(MAT_PATH, O_RDONLY);
	} else {
		printf("non blocking\n");
		ac_fd = open(ACC_PATH, O_RDONLY | O_NONBLOCK);
		ma_fd = open(MAG_PATH, O_RDONLY | O_NONBLOCK);
		mt_fd = open(MAT_PATH, O_RDONLY | O_NONBLOCK);
	}

	if (ac_fd < 0 || ma_fd < 0 || mt_fd < 0) 
		goto close;

	while (!halt) {
		int n;

		FD_ZERO(&read_set);
		usec_to_timeval(&wait, max_wait);
		FD_SET(ac_fd, &read_set);
		FD_SET(ma_fd, &read_set);
		FD_SET(mt_fd, &read_set);
		n = select(mt_fd + 1, &read_set, NULL, NULL, &wait);
		if (n < 0) {
			if (errno != EINTR) 
				continue;
		} else {
			if (FD_ISSET(ma_fd, &read_set)) {
				bytes = read(ma_fd, (char*)&mag_data, 6);
				if (bytes < 0) {
					printf("mag read error: %s\n", strerror(errno));
				}
			}
			if (FD_ISSET(ac_fd, &read_set)) {
				bytes = read(ac_fd, (char*)&acc_data, 6);
				if (bytes < 0) {
					printf("acc read error: %s\n", strerror(errno));
				}
			}
			if (FD_ISSET(mt_fd, &read_set)) {
				bytes = read(mt_fd, (char*)&mat_data, 128);
				if (bytes < 0) {
					printf("matrix read error: %s\n", strerror(errno));
				}
			}
		}	
	}
close:
	close(ac_fd);
	close(ma_fd);
	close(mt_fd);

	return 0;
}
