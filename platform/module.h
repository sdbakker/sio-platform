/**
 *  @file module.h
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

#ifndef MODULE_H
#define MODULE_H

#include "version.h"
#include "sios.h"
#include "sios_param.h"

extern struct sios_object __this_object;
extern struct sios_class * __this_class;

/**
 * This macro should be called <b>before</b> any other calls or definitions.
 *
 * MODULE_INIT defines several important housekeeping functions and declares the internal
 * object structure represented by <code>THIS_MODULE</code>.
 * @param obj The object name (deprecated)
 */
#define MODULE_INIT(obj)									\
	struct sios_object __this_object;							\
												\
	void __set_module_name(const char * name)						\
		{ if (name != NULL) strncpy(__this_object.name, name, SIOS_MAX_NAMESIZE); }	\
												\
	void __set_module_descr(const char * desc) 						\
		{ if (desc != NULL) __this_object.desc = strdup(desc); }			\
												\
	struct sios_class * __this_class;							\
	void __set_module_class(struct sios_class * class) 					\
		{ __this_class = class; }							\
												\
	unsigned int __compiled_against_version(void) 						\
		{ return SIOS_VERSION; }

#define THIS_MODULE	(&__this_object)
#define THIS_CLASS	__this_class
#define MODULE_NAME	(THIS_MODULE)->name

/**
 * Integer representation of the module version
 */
#define MODULE_VERSION	__module_version

/**
 * String representation of the module version
 */
#define MODULE_VERSION_STR __module_version_str

/**
 * Sets the module version.
 */
#define SET_MODULE_VERSION(a,b,c) 						\
	unsigned int __module_version = (((a) << 16) + ((b) << 8) + (c));	\
	char * __module_version_str = #a "." #b "." #c;

/**
 * Declare a method descriptor.
 */
#define METHOD_DESC_INITIALIZER(_n,_m,_t,_h,_d)	\
	{					\
		.obj = THIS_MODULE,		\
		.name = _n,			\
		.m_addr = _m,			\
		.typespec = _t,			\
		.handler = _h,			\
		.desc = _d,			\
	}

/**
 * Declare a parameter descriptor.
 */
#define PARAM_DESC_INITIALIZER(_n,_m,_t,_h,_d)	METHOD_DESC_INITIALIZER(_n,_m,_t,_h,_d)

/**
 * Set the modules init function.
 *
 * This function is called by the module loader on module load.
 * @param initfn the modules init function
 */
#if !defined(__APPLE__)
#define module_init(initfn) \
	int __init_module(void) __attribute__((weak, alias(#initfn)));
#else
#define module_init(initfn) \
	int __init_module(void) { return (*initfn)(); }
#endif

/**
 * Set the modules exit function.
 *
 * This function is called by the module loader on module unload.
 * @param exitfn the modules exit function
 */
#if !defined(__APPLE__)
#define module_exit(exitfn) \
	void __exit_module(void) __attribute__((weak, alias(#exitfn)));
#else
#define module_exit(exitfn) \
	void __exit_module(void) { (*exitfn)(); }
#endif

#endif
