/**
 *  @file osc.c
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

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "sios.h"
#include "osc.h"

static pthread_t udp_osc_thread;
static pthread_t tcp_osc_thread;

static lo_server udp_server = NULL;
static lo_server tcp_server = NULL;

static volatile int halt = 0;
static pthread_mutex_t halt_lock = PTHREAD_MUTEX_INITIALIZER;

static void err_handler(int num, const char *msg, const char *where)
{
	err("OSC", "%d, %s: %s", num, where, msg);
}

static void * udp_thread(void * arg)
{
	info("OSC", "udp port %d", lo_server_get_port(udp_server));
	while(!halt)
		lo_server_recv_noblock(udp_server, 10);
	return NULL;
}

static void * tcp_thread(void * arg)
{
	info("OSC", "tcp port %d", lo_server_get_port(udp_server));
	while(!halt)
		lo_server_recv_noblock(tcp_server, 10);
	return NULL;
}

int sios_osc_init(struct osc_entry * osc) 
{
	int retval;
	char sport[8];
	
	if (osc->port <= 0) return -1;
	snprintf(sport, 8, "%d", osc->port);

	dbg("osc port: %s", sport);
	if (osc->do_udp) {
		udp_server = lo_server_new_with_proto(sport, LO_UDP, err_handler);
		if (!udp_server) {
			fatal("OSC", 10, "Failed binding udp port '%d'", osc->port);
			return -1;
		}

		retval = pthread_create(&udp_osc_thread, NULL, udp_thread, NULL);
		if (retval) {
			err("OSC", "failed pthread_create");
			return retval;
		}
	}

	if (osc->do_tcp) {
		tcp_server = lo_server_new_with_proto(sport, LO_TCP, err_handler);
		if (!tcp_server) {
			fatal("OSC", 10, "Failed binding tcp port '%d'", osc->port);
			return -1;
		}

		retval = pthread_create(&tcp_osc_thread, NULL, tcp_thread, NULL);
		if (retval) {
			err("OSC", "failed pthread_create");
			return retval;
		}
	}

	return 0;
}

void sios_osc_terminate() {
	pthread_mutex_lock(&halt_lock);
	halt = 1;
	pthread_mutex_unlock(&halt_lock);
}

static int add_listener(struct sios_object * obj, lo_address addr)
{
	struct listener * l; 

	if (!addr)
		return -1;

	list_for_each_entry(l, &obj->listeners, listener) {
		if (!strcmp(lo_address_get_hostname(addr), lo_address_get_hostname(l->address)) &&
				!strcmp(lo_address_get_port(addr), lo_address_get_port(l->address))) {
			warn("OSC", "%s:%s already a listener for module %s", 
					  lo_address_get_hostname(addr),
					  lo_address_get_port(addr),
					  obj->name);
			return -1;
		}
	}

	l = (struct listener*)malloc(sizeof(struct listener));
	if (!l)
		return -1;

	INIT_LIST_HEAD(&l->listener);
	l->address = addr;
	list_add(&l->listener, &obj->listeners);

	info("OSC", "added %s:%s as listener of module %s", lo_address_get_hostname(addr), 
	 					          lo_address_get_port(addr),
							  obj->name);
	return 0;
}

static void del_listener(struct sios_object * obj, lo_address addr)
{
	struct listener * l; 
	int found = 0;

	if (!addr) return;
	list_for_each_entry(l, &obj->listeners, listener) {
		if (!strcmp(lo_address_get_hostname(addr), lo_address_get_hostname(l->address)) &&
				!strcmp(lo_address_get_port(addr), lo_address_get_port(l->address))) {
			found = 1;
			break;
		}
	}

	if (found) {
		info("OSC", "removed %s:%s as listener of module %s", 
			lo_address_get_hostname(l->address), 
		        lo_address_get_port(l->address),
			obj->name);
		list_del(&l->listener);
		lo_address_free(l->address);
		free(l);
	}
}

static lo_address fetch_address_from_handler(lo_message msg, const char * types, int argc, lo_arg **argv)
{
	lo_address addr, t = lo_message_get_source(msg);

	if (argc < 2) {
		addr = lo_address_new(lo_address_get_hostname(t), lo_address_get_port(t));
	} else {
		if (types[1] == 'i') {
			char port[6];
			snprintf(port, 6, "%d", argv[1]->i);
			addr = lo_address_new(&argv[0]->s, port);
		} else {
			addr = lo_address_new(&argv[0]->s, &argv[1]->s);
		}
	}

	return addr;
}

static int add_listen_source_handler(const char *path, const char *types, lo_arg **argv, 
			      int argc, lo_message msg, void *user_data)
{
	struct sios_method_desc * desc = (struct sios_method_desc *)user_data;
	lo_address addr;

	if (!desc || !desc->obj)
		return -1;

	addr = fetch_address_from_handler(msg, types, argc, argv);

	if (add_listener(desc->obj, addr)) {
		lo_address_free(addr);
		return -1;
	}

	return 0;
}

static int del_listen_source_handler(const char *path, const char *types, lo_arg **argv, 
			      int argc, lo_message msg, void *user_data)
{
	struct sios_method_desc * desc = (struct sios_method_desc *)user_data;
	lo_address addr;

	if (!desc || !desc->obj)
		return -1;

	addr = fetch_address_from_handler(msg, types, argc, argv);

	del_listener(desc->obj, addr);
	lo_address_free(addr);

	return 0;
}

void
sios_osc_add_listener_handlers(struct sios_object * obj) 
{
	struct sios_method_desc * add_desc, * del_desc;
	
	add_desc = sios_new_method_desc("listen", NULL, NULL, add_listen_source_handler, "start data transfer");
	del_desc = sios_new_method_desc("silence", NULL, NULL, del_listen_source_handler, "stop data transfer");
	
	if (!add_desc || !del_desc) 
		goto err;

	add_desc->obj = obj;
	del_desc->obj = obj;

	if (sios_osc_add_method_desc(add_desc)) 
		goto err;

	if (sios_osc_add_method_desc(del_desc))
		goto err;

	return;
	
err:
	warn("OSC", "failed allocating listen handlers for %s", obj->name);
}

struct sios_method_desc * sios_new_method_desc(const char * name, const char * method,
					       const char * types, osc_handler handler, 
					       const char * descr)
{
	struct sios_method_desc * desc;

	if (!name || !handler) {
		return NULL;	
	}

	desc = (struct sios_method_desc *)malloc(sizeof(*desc));
	if (!desc) {
		err("OSC", "out of memory while allocating method description");
		return NULL;
	}

	desc->typespec = NULL;
	desc->handler = handler;
	snprintf(desc->name, SIOS_MAX_NAMESIZE, name);
	if (!method || strlen(method) <= 1)
		snprintf(desc->m_addr, SIOS_MAX_NAMESIZE, name);
	else
		snprintf(desc->m_addr, SIOS_MAX_NAMESIZE, method);

	if (types) {
		desc->typespec = strdup(types);
		if (!desc->typespec) {
			free(desc->name);
			goto err;
		}
	} 

	if (descr) 
		strncpy(desc->desc, descr, SIOS_MAX_NAMESIZE);

	return desc;

err:
	free(desc);
	return NULL;
}

struct sios_param_desc * sios_new_param_desc(const char * name, const char * method, 
					     const char * types, osc_handler handler, 
					     const char * descr)
{
	return (struct sios_param_desc*)sios_new_method_desc(name, method, types, handler, descr);
}

int sios_osc_add_method_desc(struct sios_method_desc * desc)
{
	lo_method * method;
	struct sios_object * obj;
	char path[SIOS_MAX_PATHSIZE];

	if (!desc) return -1;
	if ((obj = desc->obj) == NULL) return -1;

	if (strlen(desc->m_addr) <= 1)
		snprintf(desc->m_addr, SIOS_MAX_NAMESIZE, desc->name);

	snprintf(path, SIOS_MAX_PATHSIZE, "%s/%s", obj->path, desc->m_addr);
	dbg("method path: %s", path);
	method = lo_server_add_method(udp_server, 
				      path, desc->typespec ,
				      desc->handler, desc);

	if (!method) return -1;
	
	INIT_LIST_HEAD(&desc->method);
	list_add(&desc->method, &obj->osc_methods);
	desc->lo_m = method;

	return 0;
}

int sios_osc_add_method_descs(struct sios_method_desc * desc, int cnt)
{
	int i;

	if (!desc) return -1;

	for (i=0;i<cnt;i++) 
		sios_osc_add_method_desc(&desc[i]);

	return 0;
}

int sios_osc_add_param_desc(struct sios_param_desc * desc)
{
	lo_method * method;
	struct sios_object * obj;
	char path[SIOS_MAX_PATHSIZE];

	if (!desc) return -1;
	if ((obj = desc->obj) == NULL) return -1;

	snprintf(path, SIOS_MAX_PATHSIZE, "%s/%s", obj->path, desc->name);
	method = lo_server_add_method(udp_server, path, desc->typespec,
					 desc->handler, desc);

	if (!method) return -1;
	
	INIT_LIST_HEAD(&desc->param);
	list_add(&desc->param, &obj->osc_params);
	desc->lo_m = method;

	return 0;
}

