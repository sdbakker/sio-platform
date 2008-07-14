/**
 *  @file osc.h
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

#ifndef OSC_H
#define OSC_H

#include <lo/lo.h>

#include "sios.h"
#include "util.h"

struct osc_entry;

typedef int (*osc_handler)(const char *path, const char *types, lo_arg **argv, 
		      	   int argc, lo_message msg, void *user_data);

int sios_osc_init(struct osc_entry * osc);
void sios_osc_terminate();
void sios_osc_add_listener_handlers(struct sios_object * obj); 
struct sios_method_desc * sios_new_method_desc(const char * name, const char * method,
					       const char * types, osc_handler handler, 
					       const char * descr);
struct sios_param_desc * sios_new_param_desc(const char * name, const char * method,
					     const char * types, osc_handler handler, 
					     const char * descr);
int sios_osc_add_method_desc(struct sios_method_desc * desc);
int sios_osc_add_method_descs(struct sios_method_desc * desc, int cnt);
int sios_osc_add_param_desc(struct sios_param_desc * desc);
int sios_osc_add_param_descs(struct sios_param_desc * desc, int cnt);

struct listener {
	lo_address address;
	struct list_head listener;	
};

#define sios_osc_dispatch_all(_path,_types,...) 					\
	do { 										\
		if (!list_empty(&(THIS_MODULE->listeners))) {				\
			struct listener *l;						\
			list_for_each_entry(l, &(THIS_MODULE->listeners), listener)	\
				lo_send(l->address, _path, _types, __VA_ARGS__); 	\
		}									\
	} while(0) 

#define sios_osc_dispatch(_lo_addr,_path,_types,...) lo_send(_lo_addr, _path, _types, __VA_ARGS__)

#define sios_osc_dispatch_msg_all(_path,_msg)	 					\
	do { 										\
		if (!list_empty(&(THIS_MODULE->listeners))) {				\
			struct listener *l;						\
			list_for_each_entry(l, &(THIS_MODULE->listeners), listener)	\
				lo_send_message(l->address, _path, _msg); 		\
		}									\
	} while(0) 

#define sios_osc_dispatch_msg(_lo_addr,_path,_msg) lo_send_message(_lo_addr, _path, _msg)

#endif
