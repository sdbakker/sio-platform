/**
 *  @file xmldump.h
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

#ifndef XML_DUMP_H
#define XML_DUMP_H

#include "sios.h"

#define TAG_SYS		"sys"
#define TAG_MODULE	"module"
#define TAG_DESCR	"description"
#define TAG_SYSPATH	"syspath"
#define TAG_ADDRESS	"address"
#define TAG_PARAMS	"params"
#define TAG_PARAM	"param"
#define TAG_VALUE	"value"
#define TAG_METHODS	"methods"
#define TAG_METHOD	"method"
#define TAG_CLASS	"class"
#define TAG_OSC		"osc"

#define ATTR_ID		"id"
#define ATTR_CLASS	"class"
#define ATTR_NAME	"name"
#define ATTR_VALUE	"value"
#define ATTR_ADDRESS	"address"
#define ATTR_TYPES	"types"
#define ATTR_PORT	"port"
#define ATTR_ROOT	"root"
#define ATTR_PROTO	"proto"

#include "util.h"

void sios_dump_xml(void);
void sios_dump_modules(void);
void sios_dump_classes(void);
void sios_osc_dump_xml(void);
void sios_modules_dump_xml(struct list_head * modules);
void sios_classes_dump_xml(struct list_head * classes);

#endif /* XML_DUMP_H */
