/**
 *  @file core.c
 *
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

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

#include "sios.h"
#include "sios_config.h"

static pthread_t main_reader_loop_thread;
static pthread_t main_writer_loop_thread;
static pthread_mutex_t halt_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_attr_t read_policy_attr;
static pthread_attr_t write_policy_attr;
static volatile int halt = 0;

static void * the_main_reader_loop(void * arg)
{
	info("Core", "main reader loop started");
	while (1) {
		pthread_mutex_lock(&halt_lock);
		if (halt) {
			pthread_mutex_unlock(&halt_lock);
			return NULL;
		}
		pthread_mutex_unlock(&halt_lock);

		sios_sources_execute_readers();
	}
}

static void * the_main_writer_loop(void * arg)
{
	info("Core", "main writer loop started");
	while (1) {
		pthread_mutex_lock(&halt_lock);
		if (halt) {
			pthread_mutex_unlock(&halt_lock);
			return NULL;
		}
		pthread_mutex_unlock(&halt_lock);

		sios_sources_execute_writers();
	}
}

static struct sios_source_ctx main_src_ctx = {
	.type = SIOS_TIMER,
	.priority = SIOS_PRIORITY_DEFAULT,
	.handler = NULL,
	.period = 500,
};

int sios_core_init()
{
	struct class_entry * c_entry;
	struct module_entry * m_entry;
	int retval;
	
	retval = pthread_attr_init(&read_policy_attr);
	if (retval) {
		err("Core",  "failed pthread_attr_init");
		return retval;
	}

	retval = pthread_attr_init(&write_policy_attr);
	if (retval) {
		err("Core",  "failed pthread_attr_init");
		return retval;
	}
		
	retval = pthread_attr_setschedpolicy(&read_policy_attr, SCHED_RR);
	if (retval) {
		err("Core", "failed pthread_attr_setschedpolicy");
		return retval;
	}

	retval = pthread_attr_setschedpolicy(&write_policy_attr, SCHED_RR);
	if (retval) {
		err("Core", "failed pthread_attr_setschedpolicy");
		return retval;
	}

	retval = pthread_create(&main_reader_loop_thread, NULL /*&read_policy_attr*/, 
				the_main_reader_loop, NULL);
	if (retval) {
		err("Core", "failed pthread_create");
		return retval;
	}

	retval = pthread_create(&main_writer_loop_thread, NULL /*&write_policy_attr*/, 
				the_main_writer_loop, NULL);
	if (retval) {
		err("Core", "failed pthread_create");
		return retval;
	}

/*	
	retval = sios_add_source_ctx(&main_src_ctx);
	if (retval) {
		err("Core", "failed adding main context");
		return retval;
	}
*/

	list_for_each_entry(c_entry, &config->class_entries, entry) 
		sios_class_register(c_entry->class);

	list_for_each_entry(m_entry, &config->module_entries, entry) {
		m_entry->module->class = sios_find_class_by_name(m_entry->class);
		sios_add_module(m_entry->module);
	}

	retval = sios_load_modules();
	if (retval) 
		warn("Core", "failed loading %d modules", retval);

	return 0;
}

void sios_core_exit()
{
	sios_unload_modules_all();
	sios_del_source_ctx(&main_src_ctx);

	pthread_mutex_lock(&halt_lock);
	halt = 1;
	pthread_mutex_unlock(&halt_lock);

	dbg("joining main reader thread");
	pthread_join(main_reader_loop_thread, NULL);
	dbg("joining main reader thread done");

	dbg("joining main writer thread");
	pthread_join(main_writer_loop_thread, NULL);
	dbg("joining main writer thread done");

}
