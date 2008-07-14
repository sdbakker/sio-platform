/**
 * @file object.c
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

/**
 * Doubly linked list of type list_head.
 * 
 * object_list keeps track of all registered sios_object structures.
 */
LIST_HEAD(object_list);

int sios_object_register(struct sios_object * object, struct sios_class * class)
{
	int retval = 0;

	if (!object || !class)
		return -1;
	
	INIT_LIST_HEAD(&object->class_head);
	INIT_LIST_HEAD(&object->object_head);
	INIT_LIST_HEAD(&object->listeners);
	INIT_LIST_HEAD(&object->osc_methods);
	INIT_LIST_HEAD(&object->osc_params);

	if (class) {	
		retval = sios_class_add_object(object, class);
		if (retval)
			return -1;

		snprintf(object->path, SIOS_MAX_PATHSIZE, "%s/%s", 
			 class->classpath, object->name); 
	} else {
		object->class = NULL;
		snprintf(object->path, SIOS_MAX_PATHSIZE, "%s/%s", 
			 config->osc.root, object->name); 
	}

	list_add(&object->object_head, &object_list);
	
	return 0;
}

void sios_object_deregister(struct sios_object * object)
{
	if (object->class)
		sios_class_del_object(object);

	list_del(&object->object_head);
}

void sios_object_can_have_listeners(struct sios_object * object)
{
	if (!object)
		return;
	
	sios_osc_add_listener_handlers(object);
}
