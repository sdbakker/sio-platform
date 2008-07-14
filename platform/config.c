/**
 *  @file config.c
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
#include "sios_config.h"
#include "config-parser.h"

struct kword sios_config_keywords[] = {
	{"class",		K_CLASS			},
	{"strict_versioning",	K_STRICT_VERSION	},
	{"dump_module_xml",	K_DUMP_MODULE_XML	},
	{"xml_dump_path",	K_XML_DUMP_PATH		},
	{"xml_module_prefix",	K_XML_MODULE_PREFIX	},

	{"use_syslog",		K_USE_SYSLOG		},

	{"osc",			K_OSC			},
	{"osc_port",		K_OSC_PORT		},
	{"osc_root",		K_OSC_ROOT		},
	{"osc_udp",		K_OSC_UDP		},
	{"osc_tcp",		K_OSC_TCP		},

	{"logger",		K_LOGGER		},
	{"dump",		K_DUMP			},
	{"path",		K_PATH			},
	{"prefix",		K_PREFIX		},
	{"postfix",		K_POSTFIX		},

	{"module",		K_MODULE		},
	{"module_path",		K_M_PATH		},
	{"module_class",	K_M_CLASS		},
	{"module_description",	K_M_DESC		},
	{"module_is_lazy",	K_M_LAZY		},

	{NULL, 0}
};

int sios_config_get_keyword(const char * kword) 
{
	struct kword *kw = sios_config_keywords;
	while (kw->name) {
		if (!strcasecmp(kword, kw->name)) 
			return kw->type;
		kw++;
	}
	return -1;
}
