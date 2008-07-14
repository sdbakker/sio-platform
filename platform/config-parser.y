%{
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "sios_param.h"
#include "sios.h"

extern char *current_file;
extern int current_lineno;

struct sios_config * config = NULL;
char use_syslog = 0;
static struct class_entry * config_add_class(const char * name);
static struct module_entry * config_add_module();
static int yyerror(const char *s);
int yylex();

struct module_entry * module_entry;

%}

%token K_CLASS K_MODULE K_STRICT_VERSION K_USE_SYSLOG
%token K_OSC K_OSC_PORT K_OSC_ROOT K_OSC_UDP K_OSC_TCP
%token K_DUMP_MODULE_XML K_XML_DUMP_PATH K_XML_MODULE_PREFIX
%token K_LOGGER K_DUMP K_PATH K_PREFIX K_POSTFIX
%token K_M_PATH K_M_CLASS K_M_DESC K_M_LAZY 

%union {
	char * str;
	long num;
	float fnum;
	char bool;
	struct osc_entry * osc;
	struct module_entry * entry;
	struct sios_param_type * param;
}

%token <num> NUMBER
%token <str> STRING M_PARAM
%token <fnum> FLOAT
%token <bool> BOOL

%type <osc> osc

%type <param> module_param param
%type <entry> module
%type <str> module_name 

%%

config	: /* empty */
	| statement
	| config statement
	;

statement	: K_CLASS STRING 
		{ 
			config_add_class($2);
		}
		| K_STRICT_VERSION BOOL
		{
			config->strict_versioning = $2;
		}
		| K_USE_SYSLOG 
		{
			use_syslog = 1;
		}
		| K_DUMP_MODULE_XML BOOL
		{
			config->dump_module_xml = $2;
		}
		| K_XML_DUMP_PATH STRING
		{
			config->xml_dump_path = strdup($2);
		}
		| K_XML_MODULE_PREFIX STRING
		{
			config->xml_module_prefix = strdup($2);
		}
		| osc '{' osc_options '}'
		| module module_name '{' module_options '}' 
		;

osc		: /* empty */ { $$ = NULL; }
		| K_OSC { $$ = &config->osc; }
		;

osc_options	: osc_option
		| osc_options osc_option
		;

osc_option	: K_OSC_PORT NUMBER { config->osc.port = $2; }
		| K_OSC_ROOT STRING { config->osc.root = strdup($2); }
		| K_OSC_UDP BOOL { config->osc.do_udp = $2; }
		| K_OSC_TCP BOOL { config->osc.do_tcp = $2; }
		;

module		: /* empty */ { $$ = NULL; }
		| K_MODULE
		{ 
			$$ = config_add_module();
			module_entry = $$;
		}
		;

module_name	: M_PARAM 
		{ 
			module_entry->module->module_name = strdup($1); 
			$$ = $1; 
		}
		;

module_options	: module_option
		| module_param 
		{
			sios_module_add_param(module_entry->module, $1); 
		}
		| module_options module_option
		| module_options module_param 
		{ 
			sios_module_add_param(module_entry->module, $2); 
		}
		;

module_option	: K_M_PATH STRING 
		{ 
			module_entry->module->path = strdup($2); 
		}
		| K_M_CLASS STRING 
		{ 
			module_entry->class = strdup($2);
		}
		| K_M_DESC STRING 
		{ 
			module_entry->module->module_descr = strdup($2);  
		}
		| K_M_LAZY 
		{
			module_entry->module->lazy = 1;
		}
		;

module_param	: param STRING 
		{ 
			sios_param_set_val($1, $2);
			$$ = $1;
		}
		;

param	: M_PARAM 
	{ 
		$$ = sios_param_alloc($1); 
	}
	;

%%

static int yyerror(const char *msg)
{
	err("Config", "error in file '%s' line %d: %s", current_file, current_lineno, msg);
	return 0;	
}

static struct class_entry * config_add_class(const char * name) 
{
	struct sios_class * class = sios_class_create(name);
	
	if (class) {
		struct class_entry * entry = 
			(struct class_entry*)malloc(sizeof(struct class_entry));
		if (!entry) {
			sios_class_destroy(class);
			return NULL;
		}
		INIT_LIST_HEAD(&entry->entry);
		entry->class = class;
		list_add(&entry->entry, &config->class_entries);
		return entry;
	}
	return NULL;
}


static struct module_entry * config_add_module() 
{
	struct sios_module * module = sios_alloc_module();
	
	if (module) {
		struct module_entry * entry = 
			(struct module_entry*)malloc(sizeof(struct module_entry));
		if (!entry) {
			sios_destroy_module(module);
			return NULL;
		}
		INIT_LIST_HEAD(&entry->entry);
		entry->module = module;
		list_add(&entry->entry, &config->module_entries);
		return entry;
	}
	return NULL;
}

extern FILE * yyin;

struct sios_config * sios_read_config(const char * path) 
{
	int retval;

	if (path) {
		current_file = strdup(path);
		yyin = fopen(path, "r");
		if (!yyin) {
			err("Config", "error opening %s: %s", current_file, strerror(errno));
			return NULL;
		}
	} else
		return NULL;

	current_lineno = 1;

	config = (struct sios_config *)malloc(sizeof(struct sios_config));
	if (!config)
		return NULL;

	INIT_LIST_HEAD(&config->class_entries);
	INIT_LIST_HEAD(&config->module_entries);

	retval = yyparse();
	fclose(yyin);

	free(current_file);

	return config;
}

void sios_free_config(struct sios_config * config)
{
	struct class_entry *c_entry, *c_tmp;
	struct module_entry *m_entry, *m_tmp;

	free(config->osc.root);
	
	list_for_each_entry_safe(c_entry, c_tmp, &config->class_entries, entry) {
		list_del(&c_entry->entry);
		free(c_entry);
	}

	list_for_each_entry_safe(m_entry, m_tmp, &config->module_entries, entry) {
		list_del(&m_entry->entry);
		free(m_entry->class);
		free(m_entry);
	}
}
