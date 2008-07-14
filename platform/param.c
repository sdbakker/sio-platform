/**
 *  @file param.c
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
#include "sios_param.h"

struct sios_param_type * sios_param_alloc(const char * name)
{
	struct sios_param_type * ptype;

	if (!name)
		return NULL;

	ptype = (struct sios_param_type*)malloc(sizeof(struct sios_param_type));
	if (!ptype)
		return NULL;

	ptype->name = strdup(name);
	INIT_LIST_HEAD(&ptype->params);

	return ptype;
}

void sios_param_set_val(struct sios_param_type * ptype, const char * val)
{
	if (!ptype || !val)
		return;
	ptype->val = strdup(val);
}

void sios_param_destroy(struct sios_param_type * ptype)
{
	list_del(&ptype->params);
	free(ptype->name);
	free(ptype->val);
	free(ptype);
}

struct sios_param_type * sios_param_add_param(struct sios_param_type * head, 
					      struct sios_param_type * ptype)
{
	if (!ptype || !head)
		return NULL;
	list_add(&ptype->params, &head->params);
	return head;
}

/* borrowed and modified from kernel/params.c */
#define STANDARD_PARAM_DEF(name, type, tmptype, strtolfn)      		\
	int sios_param_set_##name(const char *val, struct sios_param *p)	\
	{								\
		char *endp;						\
		tmptype l;						\
									\
		if (!val) return -1;					\
		l = strtolfn(val, &endp, 0);				\
		if (endp == val || ((type)l != l))			\
			return -1;					\
		*((type *)p->arg) = l;					\
		return 0;						\
	}

STANDARD_PARAM_DEF(byte, unsigned char, unsigned long, strtoul);
STANDARD_PARAM_DEF(short, short, long, strtol);
STANDARD_PARAM_DEF(ushort, unsigned short, unsigned long, strtoul);
STANDARD_PARAM_DEF(int, int, long, strtol);
STANDARD_PARAM_DEF(uint, unsigned int, unsigned long, strtoul);
STANDARD_PARAM_DEF(long, long, long, strtol);
STANDARD_PARAM_DEF(ulong, unsigned long, unsigned long, strtoul);

int sios_param_set_float(const char *val, struct sios_param *p)
{
	char *endp;
	double tmp;
	
	if (!val) return -1;
	tmp = strtod(val, &endp);
	if (endp == val || (float)tmp != tmp)
		return -1;
	*((float *)p->arg) = tmp;
	return 0;
}

int sios_param_set_double(const char *val, struct sios_param *p)
{
	char *endp;
	double d;
	
	if (!val) return -1;
	d = strtod(val, &endp);
	if (endp == val)
		return -1;
	*((double *)p->arg) = d;
	return 0;
}

int sios_param_set_charp(const char *val, struct sios_param *p)
{
	if (!val) {
		printf("%s: string parameter expected\n", p->name);
		return -1;
	}

	if (strlen(val) > 1024) {
		printf("%s: string parameter too long\n",
		       p->name);
		return -1;
	}

	*(char **)p->arg = (char *)val;
	return 0;
}

int sios_param_set_bool(const char *val, struct sios_param *p) {
	/* No equals means "set"... */
	if (!val) val = "1";

	/* One of =[yYtTnNfF01] */
	switch (val[0]) {
	case 'y': case 'Y': case 't': case 'T': case '1':
		*(int *)p->arg = 1;
		return 0;
	case 'n': case 'N': case 'f': case 'F': case '0':
		*(int *)p->arg = 0;
		return 0;
	}
	return -1;
}

int sios_param_set_invbool(const char *val, struct sios_param *p)
{
	int boolval, ret;
	struct sios_param dummy = { .arg = &boolval };

	ret = sios_param_set_bool(val, &dummy);
	if (ret == 0)
		*(int *)p->arg = !boolval;
	return ret;
}

int sios_param_set_copystring(const char *val, struct sios_param *p)
{
	struct sios_param_string *ps = p->arg;

	if (strlen(val)+1 > ps->maxlen) {
		printf("%s: string doesn't fit in %u chars.\n",
		       p->name, ps->maxlen-1);
		return -1;
	}
	strcpy(ps->string, val);
	return 0;
}
