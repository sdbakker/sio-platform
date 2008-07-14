/**
 *  @file sios.h
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

#ifndef SIOS_H
#define SIOS_H

#include <sys/time.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdarg.h>

#include "sios_config.h"

#ifdef ENABLE_SYSLOG

#define xprint(fd, prio, format, ...) 				\
	do { 							\
		if (use_syslog) {				\
			syslog(prio, format, ##__VA_ARGS__);	\
		} else {					\
			fprintf(fd, format "\n", ##__VA_ARGS__);	\
		}						\
	} while (0)

#define info(section,format,...) xprint(stdout, LOG_INFO, "[%s] " format, section, ##__VA_ARGS__)
#define warn(section,format,...) xprint(stdout, LOG_WARNING, "Warning: [%s] " format, section, ##__VA_ARGS__)
#define err(section,format,...) xprint(stderr, LOG_ERR, "Error: [%s] " format, section, ##__VA_ARGS__)
#define fatal(section,errcode,format,...) \
	do { \
		xprint(stderr, LOG_EMERG, "FATALITY: [%s] " format, section, ##__VA_ARGS__); \
		exit((errcode)); \
	} while (0)

#ifdef DEBUG

#define dbg(format,...) xprint(stdout, LOG_DEBUG, "Debug %s:%d in %s(): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define sassert(predicate) \
	if (!(predicate)) xprint(stderr, LOG_ALERT, "Assertion %s:%d in %s(): \"%s\"", __FILE__, __LINE__, __FUNCTION__, #predicate) 

#define sassert_fatal(predicate,errcode) \
	if (!(predicate)) { \
		xprint(stderr, LOG_EMERG, "Fatal assertion %s:%d in %s(): \"%s\"", __FILE__, __LINE__, __FUNCTION__, #predicate); \
		exit((errcode)); \
	}

#else /* DEBUG */

#define dbg(format,...)
#define sassert(predicate)
#define sassert_fatal(predicate,errcode)

#endif /* DEBUG */

#else /* ENABLE_SYSLOG */

#define info(section,format,...) fprintf(stdout, "[%s] " format "\n", section, ##__VA_ARGS__)
#define warn(section,format,...) fprintf(stdout, "Warning: [%s] " format "\n", section, ##__VA_ARGS__)
#define err(section,format,...) fprintf(stderr, "Error: [%s] " format "\n", section, ##__VA_ARGS__)
#define fatal(section,errcode,format,...) \
	do { \
		fprintf(stderr, "FATALITY: [%s] " format "\n", section, ##__VA_ARGS__); \
		exit((errcode)); \
	} while (0)

#ifdef DEBUG

#define dbg(format,...) fprintf(stdout, "Debug %s:%d in %s(): " format "\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define sassert(predicate) \
	if (!(predicate)) fprintf(stderr, "Assertion %s:%d in %s(): \"%s\"\n", __FILE__, __LINE__, __FUNCTION__, #predicate) 

#define sassert_fatal(predicate,errcode) \
	if (!(predicate)) { \
		fprintf(stderr, "Fatal assertion %s:%d in %s(): \"%s\"\n", __FILE__, __LINE__, __FUNCTION__, #predicate); \
		exit((errcode)); \
	}

#else /* DEBUG */

#define dbg(format,...)
#define sassert(predicate)
#define sassert_fatal(predicate,errcode)

#endif /* DEBUG */

#endif /* ENABLE_SYSLOG */


/**  
 * Maximum size for names used within the SIOS platform.
 */
#define SIOS_MAX_NAMESIZE	64

/** 
 * Maximum size for paths used within the SIOS platform.
 */
#define SIOS_MAX_PATHSIZE	128

struct sios_module;
struct sios_object;
struct sios_class; 
struct sios_method_desc; 
struct sios_config;

#define MODULES(m)		((m) ? (sizeof(*(m)) / sizeof(struct sios_module)) : 0)

#include "sios_param.h"
#include "osc.h"
#include "util.h"

/** 
 * Implements a module loadable by the SIOS platform.
 */
struct sios_module {
	char * basename; 		/**< Basename of the module (e.g. module_foo.so). */
	char * path;			/**< Full filesystem path to the module.*/
	int lazy;
	char * lazy_id;			/**< Triggering id for lazy load modules. */
	void * dl_handle;		/**< Opaque handle returned by dlopen(). */
	struct list_head params;	/**< list_head entry for parameters list. */
	struct list_head list;		/**< list_head entry for internal module list. */

	char * module_name;		/**< Module name. */
	char * module_descr; 		/**< Module description. */
	struct sios_class * class;	/**< Module class */
	struct sios_object * obj;	/**< The object represented by this module */

	int (*init)(void);		/**< Module init function, internal use only */
	void (*destroy)(void);		/**< Module destroy function, internal use only */
	void (*set_name)(const char * name);	/**< Sets the module name, internal use only */
	void (*set_descr)(const char * descr);	/**< Sets the module description, internal use only */
	void (*set_class)(struct sios_class * class);	/**< Sets the module class, internal use only */
};

/**
 * Allocates and initializes a sios_module structure.
 *
 * @return struct sios_module *
 */
struct sios_module * sios_alloc_module();

/**
 * Destroys a sios_module structure.
 *
 * @param module a struct sios_module *
 */
void sios_destroy_module(struct sios_module * module);

/**
 * Sets a module parameter.
 *
 * Used to set a parameter defined within the module.
 * @param module the struct sios_module * defining the parameter
 * @param ptype the parameter data
 * @return 0 on success, !0 on error
 */
int sios_module_add_param(struct sios_module * module, struct sios_param_type * ptype);

/**
 * Adds a struct sios_module * allocated by <code>sios_alloc_module()</code> to the module list.
 * 
 * This does not actually load the module. 
 * Use sios_load_module or sios_load_modules for to really load the module. 
 * @param module the struct sios_module * to add
 * @return 0 on success, !0 on error
 *
 * @see sios_alloc_module
 */
int sios_add_module(struct sios_module * module);

/**
 * Loads a dynamic loadable module.
 * 
 * If loading the module was succesfull the module's <code>module_init()</code> routine 
 * is called. This function should return 0 on success and !0 on error. If 
 * 0 was returned the module is added to the loaded module list <code>module_list</code>.
 * @param module Filled in module structure.
 * @return 0 on succes !0 on error.
 */
int sios_load_module(struct sios_module * module);

/**
 * Loads all modules added by sios_add_module.
 * 
 * This function actually loads all modules that have been previously added
 * by sios_add_module. Actually it calls <code>sios_load_module()</code> and 
 * <code>sios_init_module()</code> respectively for every module found in module_list.
 * @return 0 on success or the amount of modules that failed
 *
 * @see sios_load_module
 * @see sios_init_module
 */ 
int sios_load_modules();

/**
 * Initializes a module.
 * 
 * First this function sets the module name, description, class and parameters. Next 
 * it calls the modules init function and on success gets a handle of the initialized object.
 * @param module the struct sios_module * to initialize
 * @return 0 on success or !0 on failure
 */
int sios_init_module(struct sios_module * module);

/**
 * Unloads a sios_module from the system.
 * 
 * This function calls the <code>module_exit</code> routine, <i>free()</i>'s 
 * the module structure and removes it from the <code>module_list</code>.
 * @param module Pointer to the sios_module structure to unload.
 */
void sios_unload_module(struct sios_module * module, int call_module_destroy);

/**
 * Unloads all sios_module structures loaded into the system.
 * 
 * This routine calls @see sios_unload_module for every module 
 * in the <code>module_list</code>.
 */
void sios_unload_modules_all(void);

/** 
 * Implements the class abstraction.
 *
 * A sios_class structures objects with a certain type within a container abstract.
 * When a sios_class registeres a OSC namespace is created, objects registered with 
 * a certain class can be accessed through this namespace.
 *
 * Within the SIOS platform there are 3 main classes registerd by default:
 *  - sensors
 *  - actuators
 *  - system
 */
struct sios_class {
	char name[SIOS_MAX_NAMESIZE];		/**< Class name */
	char classpath[SIOS_MAX_PATHSIZE];	/**< The OSC path (namespace) for this class */
	struct list_head object_list;		/**< List head of registered objects */
	struct list_head class_head;		/**< list_head entry for internal class list */
};

/**
 * Creates a struct sios_class.
 *
 * sios_class_create creates a new struct_sios class initialized with
 * name. It also creates the OSC classpath and the standard /list method.
 * Note that the methods are not registered yet until sios_class_register() 
 * is called.
 *
 * @param name The name of this class.
 * @return A pointer to the new class or NULL if an error occured.
 * @see sios_class_destroy
 * @see sios_class_register
 */
struct sios_class * sios_class_create(const char * name);

/**
 * Destroys a struct sios_class created with sios_class_create().
 * 
 * sios_class_destroy <code>free()</code>'s a sios_class created with 
 * sios_class_create(). This does not remove the OSC callbacks registered by
 * sios_class_register() but does free the OSC methods structures. So do not call
 * this function before sios_class_deregister().
 *
 * @param class The struct sios_class* to free().
 * @see sios_class_create
 * @see sios_class_register
 * @see sios_class_deregister
 */
void sios_class_destroy(struct sios_class * class);

/**
 * Registers a struct sios_class created with sios_class_create() with the system.
 *
 * sios_class_register checks if such a class is not already registered and adds it 
 * to the <code>class_list</code> if not. sios_class_register also registers its 
 * default OSC methods by calling sios_osc_add_methods() on its class->methods member.
 *
 * @param class The struct sios_class* to register.
 * @return 0 on succes, !0 on error.
 * @see sios_osc_add_methods
 * @see sios_class_deregister
 */
int sios_class_register(struct sios_class * class);

/**
 * Deregisters a struct sios_class.
 *
 * sios_class_deregister deregisters a struct sios_class registered with sios_class_register().
 * It removes the OSC callbacks using sios_osc_del_methods() and removes class from the 
 * <code>class_list</code>.
 *
 * @param class The struct sios_class* to deregister.
 * @see sios_class_register
 * @see sios_osc_del_methods
 */
void sios_class_deregister(struct sios_class * class);

/**
 * Finds a registered class by name.
 *
 * sios_find_class_by_name finds a class based on its <code>class->name</code> member. 
 * The compare is case insensitive.
 *
 * @param name \0 terminated string representing the name of the class to be found.
 * @return A struct sios_class* to the matching class or NULL if no match was found.
 */
struct sios_class * sios_find_class_by_name(const char * name);

/**
 * Adds an object to a class.
 *
 * sios_class_add_object binds a sios_object to a sios_class object. The object is added to the 
 * classes <code>class->object_list</code> and the object's <code>object->class</code> is set 
 * to the class.
 *
 * @param object The struct sios_object* to be added to the class.
 * @param class The struct sios_class* to add object to.
 * @return 0 on success and !0 on error.
 */
int sios_class_add_object(struct sios_object * object, struct sios_class * class);

/**
 * Removes an object from its class.
 *
 * sios_class_del_object removes object from the class it is bound to. The class the object
 * needs to be removed from is found by looking at the <code>object->class</code> member. 
 * Afther removal the object's <code>object->class</code> member is restet to NULL.
 *
 * @param object the object to remove.
 */
void sios_class_del_object(struct sios_object * object);

/**
 * Structure describing an object within the system.
 *
 */
struct sios_object {
	char name[SIOS_MAX_NAMESIZE];	/**< Name representing the object. */
	char path[SIOS_MAX_PATHSIZE];	/**< The namespace this object resides in. */

	char *desc;	/**< Object description */
	char *id;	/**< A unique id identifying the object (e.g. a 1-wire id) */
	int type;	/**< An optional object type id */

	struct sios_class *class;	/**< The sios_class this object is bound to. */
	struct list_head class_head;	/**< list entry for the classes object list */
	struct list_head object_head; 	/**< list entry for <code>object_list</code>. @see object_list*/

	struct list_head osc_methods;	/**< list of methods exported over OSC */
	struct list_head osc_params;	/**< list of parameters exported over OSC */

	struct list_head listeners;	/**< list of registered listeners if any */
};

/**
 * Register a sios_object.
 *
 * sios_object_register registers a <code>struct sios_object</code> with the system and binds it 
 * to a <code>struct sios_class</code> using sios_class_add_object(). It setups the objects OSC 
 * namespace, which is a concatenation of its classes classpath and the object's name. All OSC methods
 * are added under this namespace. As last the object is added to the <code>object_list</code>.
 *
 * @param object The struct sios_object* to add.
 * @param class The struct sios_class* this object will be bound to.
 * @return 0 on success, !0 on error.
 *
 * @see sios_class_add_object
 * @see sios_osc_add_methods
 */
int sios_object_register(struct sios_object * object, struct sios_class * class);

/** 
 * Deregister a sios_object.
 *
 * sios_object_deregister removes a <code>struct sios_object</code> from the system. 
 * It does not deallocate any resources.
 * @param The struct sios_object* to deregister
 * @return 0 on success, !0 on failure
 */
void sios_object_deregister(struct sios_object * object);

/**
 * Register listener osc methods.
 *
 * sios_object_can_have_listeners creates the OSC methods <i>listen</i> and <i>silence</i>
 * for the object and associates the handling callbacks with it.
 * @param object The struct sios_object* for which to add the methods.
 */
void sios_object_can_have_listeners(struct sios_object * object);

/**
 * Structure describing an exported OSC method.
 */
struct sios_method_desc {
	struct sios_object * obj;		/**< The owning sios_object */
	char name[SIOS_MAX_NAMESIZE];		/**< Method name */
	char m_addr[SIOS_MAX_NAMESIZE];		/**< Method address */
	char desc[SIOS_MAX_NAMESIZE];		/**< Method description */
	char * typespec;			/**< OSC type specification */
	osc_handler handler;			/**< Method handler */
	lo_method lo_m;				/**< Liblo method implementation */
	struct list_head method;		/**< list head entry used to register method */
	void * priv;				/**< private data */
};

/**
 * Structure describing an exported OSC parameter.
 * 
 * Internally parameters and methods share the same OSC implementation.
 */
struct sios_param_desc {
	struct sios_object * obj;		/**< The owning sios_object */
	char name[SIOS_MAX_NAMESIZE];		/**< Parameter name */
	char m_addr[SIOS_MAX_NAMESIZE];		/**< Parameter address */
	char desc[SIOS_MAX_NAMESIZE];		/**< Parameter description */
	char * typespec;			/**< OSC type specification */
	osc_handler handler;			/**< Parameter handler */
	lo_method lo_m;				/**< Liblo method implementation */
	struct list_head param;			/**< list head entry used to register parameter */
	void * priv;				/**< private data */
};

#define METHOD_DESCRIPTORS(_md)	(sizeof((_md)) / sizeof(struct sios_method_desc))
#define PARAM_DESCRIPTORS(_pd)	(sizeof((_pd)) / sizeof(struct sios_param_desc))

enum sios_source_type {
	SIOS_POLL_NONE	= 0, 
	SIOS_POLL_READ 	= 1,
	SIOS_POLL_WRITE	= 2,
	SIOS_TIMER	= 4,
};

enum sios_event_type {
	SIOS_EVENT_READ		= 0,
	SIOS_EVENT_WRITE	= 1,
	SIOS_EVENT_TIMEOUT	= 2,
};

enum sios_source_priority {
	SIOS_PRIORITY_MAX	= -999,
	SIOS_PRIORITY_HIGH	= -100,
	SIOS_PRIORITY_DEFAULT	= 0,
	SIOS_PRIORITY_LOW	= 100,
};

/**
 * Describes a running source context.
 *
 * A source context describes the read, write and/or timer events a
 * sios_object wants to react upon. The event handlers should be treated as 
 * interrupt handlers and should not block or sleep.
 */
struct sios_source_ctx {
	struct sios_object * self;					/**< sios_object registering the source context */
	enum sios_source_type type;					/**< An OR'ed combination of <code>SIOS_POLL_NONE, SIOS_POLL_READ, SIOS_POLL_WRITE, SIOS_TIMER</code> */
	enum sios_source_priority priority;				/**< Handling priority: -999 for highest priority, 100 for lowest */ 
	int (*handler)(struct sios_source_ctx *, enum sios_event_type);	/**< The event handler callback */
	long period;							/**< timer period in us */
	long elapsed;							/**< elapsed time since last event in us */
	int fd;								/**< file descriptor for read or write events */
	struct list_head ctx_reader_head;				/**< list_head entry for reader thread */
	struct list_head ctx_writer_head;				/**< list_head entry for writer thread */
	void * priv;							/**< private data */
};

/**
 * Execute a single writer loop.
 * 
 * This computes the time periods and delays and calls select on the registered
 * writers with the minimum timeout.
 */
void sios_sources_execute_writers(void);

/**
 * Execute a single reader loop.
 *
 * This computes the time periods and delays and calls select on the registered
 * readers with the minimum timeout.
 */
void sios_sources_execute_readers(void);

/**
 * Checks if a sios_source_ctx is already registered.
 *
 * @param ctx The sios_source_ctx
 * @return 0 if none found, 1 if context exists
 */
int sios_source_ctx_exists(struct sios_source_ctx * ctx); 

/**
 * Adds a sios_source_ctx.
 *
 * @param ctx The sios_source_ctx to add
 * @return 0 on succes, !0 on failure
 */
int sios_add_source_ctx(struct sios_source_ctx * ctx);

/**
 * Removes a sios_source_ctx.
 *
 * @param ctx The sios_source_ctx to remove
 */
void sios_del_source_ctx(struct sios_source_ctx * ctx);

/**
 * Prints all active contexts.
 */
void print_sources_list(void);


/**
 * Initialize and start the SIOS core.
 */
int sios_core_init(void);

/**
 * Deinitialize and stop the SIOS core
 */
void sios_core_exit(void);

#endif
