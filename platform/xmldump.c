/**
 *  @file xmldump.c
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "sios.h"
#include "xmldump.h"
#include "sios_config.h"

void sios_dump_xml(void)
{
	if (config->xml_dump_path) {
		sios_dump_classes();
		sios_dump_modules();
		sios_osc_dump_xml();
	}
}

void sios_osc_dump_xml(void)
{
	char * dumppath;
	char linebuf[1024];
	int buflen;
	int pathlen;
	int fd;

	pathlen = strlen(config->xml_dump_path) + 9;
	dumppath = (char*)malloc(pathlen);
	snprintf(dumppath, pathlen, "%s/osc.xml", config->xml_dump_path);
	
	fd = open(dumppath, O_WRONLY|O_CREAT, 0755);
	if (fd < 0) {
		warn("XMLDUMP", "Could not open xml dump path '%s': %s", dumppath, strerror(errno));
		free(dumppath);
		return;
	}	

	if (config->osc.do_udp) {
		buflen = sprintf(linebuf, "<%s %s=\"%d\" %s=\"%s\" %s=\"%s\"/>\n", 
					  TAG_OSC, 
					  ATTR_PORT, config->osc.port,
					  ATTR_PROTO, "udp",
					  ATTR_ROOT, config->osc.root);
		write(fd, linebuf, buflen);
		bzero(linebuf, buflen);
	}
	
	if (config->osc.do_tcp) {
		buflen = sprintf(linebuf, "<%s %s=\"%d\" %s=\"%s\" %s=\"%s\"/>\n", 
					  TAG_OSC, 
					  ATTR_PORT, config->osc.port,
					  ATTR_PROTO, "tcp",
					  ATTR_ROOT, config->osc.root);
		write(fd, linebuf, buflen);
		bzero(linebuf, buflen);
	}	

	free(dumppath);
	close(fd);
}

void sios_classes_dump_xml(struct list_head * classes) 
{
	struct sios_class * class;
	char * dumppath;
	char linebuf[1024];
	int buflen;
	int pathlen;
	int fd;

	if (list_empty(classes)) return;

	pathlen = strlen(config->xml_dump_path) + 13;
	dumppath = (char*)malloc(pathlen);
	snprintf(dumppath, pathlen, "%s/classes.xml", config->xml_dump_path);
	
	fd = open(dumppath, O_WRONLY|O_CREAT, 0755);
	if (fd < 0) {
		warn("XMLDUMP", "Could not open xml dump path '%s': %s", dumppath, strerror(errno));
		free(dumppath);
		return;
	}	

	list_for_each_entry(class, classes, class_head) {
		if (!class) continue;
		buflen = sprintf(linebuf, "<%s %s=\"%s\"/>\n", TAG_CLASS, ATTR_NAME, class->name);
		write(fd, linebuf, buflen);
		bzero(linebuf, buflen);
	}
	
	free(dumppath);
	close(fd);
}

void sios_modules_dump_xml(struct list_head * modules) 
{
	struct sios_module * module;
	
	if (list_empty(modules)) return;
	
	list_for_each_entry(module, modules, list) {
		struct sios_object * obj;
		struct sios_method_desc * m_desc;
		struct sios_param_desc * p_desc;
		char * dumppath;
		char linebuf[1024];
		int buflen;
		int pathlen;
		int fd;

		if (!module) continue;
		
		obj = module->obj;

		pathlen = strlen(config->xml_dump_path) 
			  + ((config->xml_module_prefix) ? strlen(config->xml_module_prefix) : 0)
			  + strlen(obj->name) + 6;

		dumppath = (char*)malloc(pathlen);
		snprintf(dumppath, pathlen, "%s/%s%s.xml", config->xml_dump_path, config->xml_module_prefix, obj->name);
		
		fd = open(dumppath, O_WRONLY|O_CREAT, 0755);
		if (fd < 0) {
			warn("XMLDUMP", "Could not open xml dump path '%s': %s", dumppath, strerror(errno));
			free(dumppath);
			return;
		}
		
		/* module */
		buflen = sprintf(linebuf, "<%s %s=\"%s\" %s=\"%s\">\n", TAG_MODULE, 
									ATTR_CLASS, obj->class->name, 
									ATTR_NAME, obj->name);
		write(fd, linebuf, buflen);
		bzero(linebuf, buflen);

		/* description */
		buflen = sprintf(linebuf, "\t<%s>%s</%s>\n", TAG_DESCR, obj->desc, TAG_DESCR);
		write(fd, linebuf, buflen);
		bzero(linebuf, buflen);

		/* syspath */
		buflen = sprintf(linebuf, "\t<%s %s=\"%s\"/>\n", TAG_SYSPATH, ATTR_VALUE, module->path);
		write(fd, linebuf, buflen);
		bzero(linebuf, buflen);

		/* address */
		buflen = sprintf(linebuf, "\t<%s %s=\"%s\"/>\n", TAG_ADDRESS, ATTR_VALUE, obj->path);
		write(fd, linebuf, buflen);
		bzero(linebuf, buflen);

		/* params */
		if (!list_empty(&obj->osc_params)) {
			buflen = sprintf(linebuf, "\t<%s>\n", TAG_PARAMS);
			write(fd, linebuf, buflen);
			bzero(linebuf, buflen);

			list_for_each_entry(p_desc, &obj->osc_params, param) {
				buflen = sprintf(linebuf, "\t\t<%s %s=\"%s\" %s=\"%s/%s\" %s=\"%s\">\n", 
								TAG_PARAM,
								ATTR_NAME, p_desc->name,
								ATTR_ADDRESS, obj->path, p_desc->m_addr,
								ATTR_TYPES, (p_desc->typespec) ? p_desc->typespec : "");
				write(fd, linebuf, buflen);
				bzero(linebuf, buflen);

				/* description */
				buflen = sprintf(linebuf, "\t\t\t<%s>%s</%s>\n", TAG_DESCR, p_desc->desc, TAG_DESCR);
				write(fd, linebuf, buflen);
				bzero(linebuf, buflen);
			
				buflen = sprintf(linebuf, "\t\t</%s>\n", TAG_PARAM);
				write(fd, linebuf, buflen);
				bzero(linebuf, buflen);

			}
			buflen = sprintf(linebuf, "\t</%s>\n", TAG_PARAMS);
			write(fd, linebuf, buflen);
			bzero(linebuf, buflen);
		}
		
		/* methods */
		if (!list_empty(&obj->osc_methods)) {
			buflen = sprintf(linebuf, "\t<%s>\n", TAG_METHODS);
			write(fd, linebuf, buflen);
			bzero(linebuf, buflen);

			list_for_each_entry(m_desc, &obj->osc_methods, method) {
				buflen = sprintf(linebuf, "\t\t<%s %s=\"%s\" %s=\"%s/%s\" %s=\"%s\">\n", 
								TAG_METHOD,
								ATTR_NAME, m_desc->name,
								ATTR_ADDRESS, obj->path, m_desc->m_addr,
								ATTR_TYPES, (m_desc->typespec) ? m_desc->typespec : "");
				write(fd, linebuf, buflen);	
				bzero(linebuf, buflen);

				/* description */
				buflen = sprintf(linebuf, "\t\t\t<%s>%s</%s>\n", TAG_DESCR, m_desc->desc, TAG_DESCR);
				write(fd, linebuf, buflen);
				bzero(linebuf, buflen);
			
				buflen = sprintf(linebuf, "\t\t</%s>\n", TAG_METHOD);
				write(fd, linebuf, buflen);
				bzero(linebuf, buflen);

			}
			buflen = sprintf(linebuf, "\t</%s>\n", TAG_METHODS);
			write(fd, linebuf, buflen);
			bzero(linebuf, buflen);
		}


		/* close module */
		buflen = sprintf(linebuf, "</%s>\n", TAG_MODULE);
		write(fd, linebuf, buflen);
		bzero(linebuf, buflen);

		free(dumppath);
		close(fd);
	}
}
