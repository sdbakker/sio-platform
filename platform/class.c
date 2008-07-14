/**
 * @file class.c
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
#include <stdlib.h>
#include <string.h>

#include "sios.h"
#include "sios_config.h"

/**
 * Doubly linked list of type list_head.
 * 
 * class_list keeps track of all registered sios_class structures.
 */
LIST_HEAD(class_list);


struct sios_class * sios_find_class_by_name(const char * name)
{
	struct sios_class *ptr;

	if (!name)
		return NULL;

	list_for_each_entry(ptr, &class_list, class_head) {
		if (!strcasecmp(ptr->name, name)) 
			return ptr;
	}

	return NULL;
}

struct sios_class * sios_class_create(const char * name)
{
	struct sios_class *class;

	if (!name)
		return NULL;
	
	info("Class", "creating class '%s'", name);
	class = (struct sios_class*)malloc(sizeof(struct sios_class));
	if (!class)
		return NULL;

	snprintf(class->name, SIOS_MAX_NAMESIZE, "%s", name);
	INIT_LIST_HEAD(&class->class_head);
	INIT_LIST_HEAD(&class->object_list);

	return class;
}

void sios_class_destroy(struct sios_class * class)
{
	if (class) {
		free(class->name);
		free(class);
	}
}

int sios_class_register(struct sios_class * class)
{
	if (!class)
		return -1;
	
	info("Class", "registering class '%s'", class->name);
	
	if (sios_find_class_by_name(class->name)) {
		warn("Class", "'%s' already exists", class->name);
		return -1;
	}

	snprintf(class->classpath, SIOS_MAX_PATHSIZE, "%s/%s", 
			config->osc.root, class->name);

	list_add(&class->class_head, &class_list);

	return 0;
}

void sios_class_deregister(struct sios_class * class)
{
	if (!class || !list_empty(&class->object_list))
		return;

	list_del(&class->class_head);
}

int sios_class_add_object(struct sios_object * object, struct sios_class * class)
{
	if (!object || !class)
		return -1;

	object->class = class;
	list_add(&object->class_head, &class->object_list);
	
	return 0;
}

void sios_class_del_object(struct sios_object * object)
{
	if (!object)
		return;
	object->class = NULL;
	list_del(&object->class_head);
}

void sios_dump_classes(void)
{
	sios_classes_dump_xml(&class_list);
}
