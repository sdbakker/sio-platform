/**
 *  @file main.c
 *
 *  SIOS platform
 *  Copyright (C) 2006 V2_lab, Simon de Bakker <simon@v2.nl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <sys/signal.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

#include "sios.h"
#include "version.h"

#include "osc.h"
#include "xmldump.h"

#define DEFAULT_CONFIGURE_PATH	"/etc/sios.config"

static volatile short halt = 0;

static void handle_sigint(int sigraised)
{
	sassert(sigraised == SIGINT || sigraised == SIGQUIT);
	info("Core", "Caught SIGINT/SIGQUIT, exiting...\n");
	halt = 1;
}

static void usage(const char * name) {
	printf("Usage: sios [OPTIONS]\n\n");
	printf("  -p, --osc_port\t\t\tOSC server port\n");
	printf("  -f, --config\t\t\tconfiguration file (default: %s)\n", DEFAULT_CONFIGURE_PATH);
	printf("  -h, --help\t\t\tThis help message\n");
	printf("  -V, --version\t\t\tSIOS version\n");
	printf("\n");
	exit(1);
}

static void main_cleanup(void)
{
	sios_core_exit();
	sios_osc_terminate();
}

int main(int argc, char * argv[])
{
	char config_file[128] = DEFAULT_CONFIGURE_PATH;
	int osc_port = 0;
	int retval;

	signal(SIGINT, handle_sigint);
	signal(SIGQUIT, handle_sigint);

	while(1) {
		static int c;
		int option_index = 0;
		static struct option long_options[] = {
			{"osc_port", 1, 0, 'p'},
			{"config", 1, 0, 'f'},
			{"version", 0, 0, 'V'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "p:f:Vh", long_options, &option_index);
		if (c<0)
			break;
		switch(c) {
			case 'p':
				osc_port = atoi(optarg);
				break;
			case 'f':
				strncpy(config_file, optarg, 128);
				break;
			case 'V':
				printf(SIOS_VERSION_STR "\n");
				exit(0);
			case 'h':
			case '?':
				usage(argv[0]);
			default:
				break;
		}
	}

	printf("\n\tStarting SIOS version %s\n\n", SIOS_VERSION_STR);

	if (!sios_read_config(config_file)) 
		fatal("Main", 1, "Error reading configuration");
	
	if (use_syslog) 
		openlog("sios", LOG_CONS|LOG_PERROR|LOG_PID, LOG_USER);

	/* commandline port overrules config */
	if (osc_port > 0) config->osc.port = osc_port;
	retval = sios_osc_init(&config->osc);
	
	if (retval) 
		fatal("Main", retval, "Initializing OSC failed");

	retval = sios_core_init();
	if (retval)
		fatal("Main", retval, "Initializing core failed");

	if (config->dump_module_xml) 
		sios_dump_xml();

	while (!halt) 
		sleep(1);
	
	main_cleanup();

	if (use_syslog)
		closelog();

	return 0;
}
