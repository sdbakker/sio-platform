/**
 *  @file sios_config.h
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

#ifndef CONFIG_H
#define CONFIG_H

#include "util.h"
#include "sios.h"

struct class_entry {
	struct sios_class * class;
	struct list_head entry;
};

struct module_entry {
	struct sios_module * module;
	char * class;
	struct list_head entry;
};

struct osc_entry {
	char * root;
	int port;
	char do_udp;
	char do_tcp;
};

struct kword {
	char * name;
	int type;
};

struct sios_config {
	struct osc_entry osc;
	char strict_versioning;
	char dump_module_xml;
	char * xml_dump_path;
	char * xml_module_prefix;
	struct list_head class_entries;
	struct list_head module_entries;
	struct list_head dump_enties;
};

extern struct kword sios_config_kewords[];

int sios_config_get_keyword(const char * kword);
struct sios_config * sios_read_config(const char * path);
void sios_free_config(struct sios_config * config);

extern struct sios_config * config;
extern char use_syslog;

#endif
