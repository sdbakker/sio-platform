/**
 *  @file module.c
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

#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <libgen.h>
#include <limits.h>

#define _GNU_SOURCE
#include <string.h>

#include "sios.h"
#include "version.h"
#include "sios_config.h"
#include "xmldump.h"

/**
 * Doubly linked list of type list_head
 * 
 * module_list keeps track of all loaded sios_module structures.
 */
LIST_HEAD(module_list);
LIST_HEAD(lazy_module_list);

static int module_set_params(struct sios_module * module)
{
	struct sios_param_type * param = NULL;
	int retval = 0;

	if (!module->dl_handle)
		return -1;
	
	list_for_each_entry(param, &module->params, params) {
		char param_func_name[SIOS_MAX_NAMESIZE];
		int (*param_set_func)(const char * val);

		snprintf(param_func_name, SIOS_MAX_NAMESIZE, 
			 "__param_set_%s", param->name);
		param_set_func = dlsym(module->dl_handle, param_func_name);
		retval += param_set_func(param->val);
	}

	return retval;
}

struct sios_module * sios_alloc_module() 
{
	struct sios_module * module;

	module = (struct sios_module*)malloc(sizeof(struct sios_module));
	if (!module)
		return NULL;

	INIT_LIST_HEAD(&module->params);
	INIT_LIST_HEAD(&module->list);
	
	return module;
}

void sios_destroy_module(struct sios_module * module)
{
	struct sios_param_type *tmp_param, *param;
	
	list_for_each_entry_safe(param, tmp_param, &module->params, params) {
		sios_param_destroy(param);
	}

	free(module->basename);
	free(module->path);
	free(module->module_name);
	free(module->module_descr);

	free(module);
}

int sios_module_add_param(struct sios_module * module, struct sios_param_type * ptype)
{
	if (!ptype)
		return -1;
	list_add(&ptype->params, &module->params);

	return 0;
}

int sios_add_module(struct sios_module * module)
{
	struct sios_module * m;
	char * bname;
	struct list_head * mlist;

	if (module->lazy)
		mlist = &lazy_module_list;
	else
		mlist = &module_list;
	
	bname = (char*)basename((char*)module->path);

#if !defined(__APPLE__)
	module->basename = (char*)strndup(bname, 256);
#else
	module->basename = (char*)strdup(bname);
#endif

	if (!module->basename)
		return -1;
	
	list_for_each_entry(m, mlist, list) {
		if (!strcmp(module->basename, m->basename)) {
			warn("Module", "'%s' already loaded", module->basename);
			return -1;
		}
	}	
	
	INIT_LIST_HEAD(&module->list);
	list_add(&module->list, mlist);

	return 0;
}

int sios_load_modules()
{
	int retval = 0;
	struct sios_module *module, *tmp;

	list_for_each_entry_safe(module, tmp, &module_list, list) {
		if(sios_load_module(module)) {
			warn("Module", "error loading module: '%s'", module->module_name);
			retval++;
		} else if(sios_init_module(module)) {
			warn("Module", "error initializing module: '%s'", module->module_name);
			sios_unload_module(module, 0);
			sios_destroy_module(module);
			retval++;
		} 
	}

	return retval;
}

int sios_load_module(struct sios_module * module)
{
	int retval = -1;
	unsigned int compiled_version;
	unsigned int (*get_version)(void);
	char ** version_str;


	module->dl_handle = dlopen(module->path, RTLD_NOW);
	if (!module->dl_handle) {
		err("Module", "dlopen() failed: %s", dlerror());
		goto out;
	}

	get_version = dlsym(module->dl_handle, "__compiled_against_version");
	if (!get_version)
		goto out_close_handle;
	
	compiled_version = get_version();
	if (compiled_version != SIOS_VERSION) {
		if (config->strict_versioning) {
			/* make version mismatch an error and unload module */
			err("Module", "module version mismatch, %s has SIOS version %d, should be %d",
				module->module_name, compiled_version,
				SIOS_VERSION);
			goto out_close_handle;
		} else {
			/* make wrong version a warning and continue loading */
			warn("Module", "module version mismatch, %s has SIOS version %d, should be %d",
				module->module_name, compiled_version,
				SIOS_VERSION);
		}
	}

	version_str = dlsym(module->dl_handle, "__module_version_str");
	if (version_str)
		info("Module", "loading module '%s' version %s", module->basename, *version_str);
	else
		info("Module", "loading module '%s'", module->basename);

	module->init = dlsym(module->dl_handle, "__init_module");
	module->destroy = dlsym(module->dl_handle, "__exit_module");
	module->set_name = dlsym(module->dl_handle, "__set_module_name");
	module->set_descr = dlsym(module->dl_handle, "__set_module_descr");
	module->set_class = dlsym(module->dl_handle, "__set_module_class");
	if (!module->init || !module->destroy || !module->set_name 
			|| !module->set_descr || !module->set_class) 
		goto out_close_handle;

	return 0;

out_close_handle:
	dlclose(module->dl_handle);
	list_del(&module->list);
out:
	return retval;
}

int sios_init_module(struct sios_module * module)
{
	int retval;
	struct sios_object * obj;

	if (!module)
		return -1;

	module->set_name(module->module_name);
	module->set_descr(module->module_descr);
	module->set_class(module->class);
	
	if (module_set_params(module)) {
		err("Module", "Error setting parameters '%s'", module->module_name);
		return -1;
	}

	retval = module->init();
	if (retval) {
		err("Module", "Error initializing '%s'", module->module_name);
		return retval;
	}

	obj = dlsym(module->dl_handle, "__this_object");
	if (!obj) {
		err("Module", "Error getting object '%s'", module->module_name);
		return -1;
	}

	module->obj = obj;

	return 0;
}

void sios_unload_module(struct sios_module * module, int call_module_destroy)
{
	if (call_module_destroy)
		module->destroy();	
	dlclose(module->dl_handle);
	
	list_del(&module->list);
}

void sios_unload_modules_all(void)
{
	struct sios_module *mptr, *tmp;

	list_for_each_entry_safe(mptr, tmp, &module_list, list) {
		info("Module", "Unloading module '%s'", mptr->basename);
		sios_unload_module(mptr, 1);
	}
}

void sios_dump_modules(void)
{
	sios_modules_dump_xml(&module_list);
}
