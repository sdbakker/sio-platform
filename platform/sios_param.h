/**
 *  @file sios_param.h
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

#ifndef SIOS_PARAM_H
#define SIOS_PARAM_H

#include "util.h"

struct sios_param; 
typedef int (*sios_param_set_fn)(const char * val, struct sios_param *);

struct sios_param_type {
	char * name;
	char * val;
	struct list_head params;
};

struct sios_param {
	char * name;
	sios_param_set_fn set;
	void * arg;
};

struct sios_param_string {
	int maxlen;
	char * string;
};

#define sios_param_call(name, val)				\
	(__sios_param_##name).set(val, &__sios_param_##name)

#define __sios_param(name, setfn, value)				\
	static char __sios_param_str_##name[] = #name;		\
	static struct sios_param __sios_param_##name		\
		= { __sios_param_str_##name, setfn, &value };	\
	int __param_set_##name(char *val)			\
	{							\
		return __sios_param_##name.set(val, 		\
				&__sios_param_##name);		\
	}							\

/**
 * Define a module parameter.
 *
 * @param name parameter name
 * @param type parameter type
 * @param value default value
 */
#define sios_param(name, type, value)	\
	__sios_param(name, sios_param_set_##type, value)

/**
 * Define a module string parameter.
 *
 * @param name parameter name
 * @param type default value
 * @param value maximum string length (including null byte)
 */
#define sios_param_string(name, string, len)	\
	static struct sios_param_string __sios_param_string_##name	\
		= { len, string };					\
	__sios_param(name, sios_param_set_copystring, __sios_param_string_##name)

extern int sios_param_set_byte(const char *val, struct sios_param *p);
extern int sios_param_set_short(const char *val, struct sios_param *p);
extern int sios_param_set_ushort(const char *val, struct sios_param *p);
extern int sios_param_set_int(const char *val, struct sios_param *p);
extern int sios_param_set_uint(const char *val, struct sios_param *p);
extern int sios_param_set_long(const char *val, struct sios_param *p);
extern int sios_param_set_ulong(const char *val, struct sios_param *p);
extern int sios_param_set_float(const char *val, struct sios_param *p);
extern int sios_param_set_double(const char *val, struct sios_param *p);
extern int sios_param_set_charp(const char *val, struct sios_param *p);
extern int sios_param_set_bool(const char *val, struct sios_param *p);
extern int sios_param_set_invbool(const char *val, struct sios_param *p);
extern int sios_param_set_copystring(const char *val, struct sios_param *p);

struct sios_param_type * sios_param_alloc(const char * name);
void sios_param_set_val(struct sios_param_type * ptype, const char * val);
void sios_param_destroy(struct sios_param_type * ptype);
struct sios_param_type * sios_param_add_param(struct sios_param_type * head,
					      struct sios_param_type * ptype);

#endif
