/**
 *  @file source.c
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
#include <errno.h>
#include <limits.h>

#include "timediff.h"
#include "util.h"
#include "sios.h"

LIST_HEAD(readers_list);
static pthread_mutex_t readers_list_lock = PTHREAD_MUTEX_INITIALIZER;
LIST_HEAD(writers_list);
static pthread_mutex_t writers_list_lock = PTHREAD_MUTEX_INITIALIZER;

static void add_source_ctx_unlocked(struct sios_source_ctx * ctx)
{
	struct list_head * ptr;

	if (ctx->type & SIOS_POLL_READ) {
		/* place in list, highest priority first (lowest number) */
		list_for_each(ptr, &readers_list) {
			struct sios_source_ctx * entry;
			entry = container_of(ptr, struct sios_source_ctx, ctx_reader_head);
			if (ctx->priority <= entry->priority) {
				__list_add(&ctx->ctx_reader_head, ptr->prev, ptr);
				return;
			}
		}
		list_add_tail(&ctx->ctx_reader_head, &readers_list);
	}

	if (ctx->type & SIOS_POLL_WRITE) {
		/* place in list, highest priority first (lowest number) */
		list_for_each(ptr, &writers_list) {
			struct sios_source_ctx * entry;
			entry = container_of(ptr, struct sios_source_ctx, ctx_writer_head);
			if (ctx->priority <= entry->priority) {
				__list_add(&ctx->ctx_writer_head, ptr->prev, ptr);
				return;
			}
		}
		list_add_tail(&ctx->ctx_writer_head, &writers_list);
	}
}

static void del_source_ctx_unlocked(struct sios_source_ctx * ctx)
{
	struct sios_source_ctx * ptr;

	if (ctx->type & SIOS_POLL_READ) {
		list_for_each_entry(ptr, &readers_list, ctx_reader_head) {
			if (ptr == ctx) {
				list_del_init(&ptr->ctx_reader_head);
				return;
			}
		}
	}

	if (ctx->type & SIOS_POLL_WRITE) {
		list_for_each_entry(ptr, &writers_list, ctx_writer_head) {
			if (ptr == ctx) {
				list_del_init(&ptr->ctx_writer_head);
				return;
			}
		}
	}
}

/* only call this function from within a locked context 
 * as it may alter the sources_list */
static inline void call_context_handler(struct sios_source_ctx * ctx, enum sios_event_type action)
{
//	dbg("calling %s", ctx->self->name);
	if (ctx->handler && ctx->handler(ctx, action)) 
		del_source_ctx_unlocked(ctx);
//	dbg("done calling");
	ctx->elapsed = 0;
}

void sios_sources_execute_writers(void)
{
	int n, max_fd = 0;
	struct sios_source_ctx * ctx, * tmp;
	struct timeval wait, start, stop, dT;
	
	static fd_set write_set;
	static suseconds_t elapsed_wait = 0L;
	suseconds_t max_wait = 10000;

	FD_ZERO(&write_set);
	
	pthread_mutex_lock(&writers_list_lock);
	
	list_for_each_entry_safe(ctx, tmp, &writers_list, ctx_writer_head) {
		suseconds_t diff = LONG_MAX; 
		
		if (ctx->type & SIOS_POLL_WRITE) {
			if (ctx->period) {
				if (ctx->elapsed >= ctx->period) {
					FD_SET(ctx->fd, &write_set);
					ctx->elapsed = 0;
				}
			} else {
				FD_SET(ctx->fd, &write_set);
			}
			max_fd = (ctx->fd > max_fd) ? ctx->fd : max_fd;
		}
		
		if (ctx->period)
			diff = ctx->period - ctx->elapsed;
		if (max_wait > diff)
			max_wait = diff;
	}
	
	pthread_mutex_unlock(&writers_list_lock);

	usec_to_timeval(&wait, max_wait);
	gettimeofday(&start, NULL);
//	dbg("pre-select");
	n = select(max_fd + 1, NULL, &write_set, NULL, &wait);
//	dbg("post-select");
	if (n < 0) {
		if (errno != EINTR)
			return;
	} else {
		gettimeofday(&stop, NULL);
		timeval_subtract(&dT, &stop, &start);
		elapsed_wait = timeval_to_usec(&dT);

//		dbg("pre lock");
		pthread_mutex_lock(&writers_list_lock);
		list_for_each_entry_safe(ctx, tmp, &writers_list, ctx_writer_head) {
			ctx->elapsed += elapsed_wait;
			if (FD_ISSET(ctx->fd, &write_set)) {
				call_context_handler(ctx, SIOS_EVENT_WRITE); 
			}
			if (ctx->type & SIOS_TIMER && ctx->elapsed >= ctx->period) 
				call_context_handler(ctx, SIOS_EVENT_TIMEOUT);
		}
		pthread_mutex_unlock(&writers_list_lock);
//		dbg("post lock");
	}
}
void sios_sources_execute_readers(void)
{
	int n, max_fd = 0;
	struct sios_source_ctx * ctx, * tmp;
	struct timeval wait, start, stop, dT;
	
	static fd_set read_set;
	static suseconds_t elapsed_wait = 0L;
	suseconds_t max_wait = 500;

	FD_ZERO(&read_set);
	
	pthread_mutex_lock(&readers_list_lock);
	
	list_for_each_entry_safe(ctx, tmp, &readers_list, ctx_reader_head) {
		//suseconds_t diff = LONG_MAX; 
		if (ctx->type & SIOS_POLL_READ) {
			FD_SET(ctx->fd, &read_set);
			max_fd = (ctx->fd > max_fd) ? ctx->fd : max_fd;
		}
	/*
		if (ctx->period)
			diff = ctx->period - ctx->elapsed;
		if (max_wait > diff)
			max_wait = diff;
	*/
	}
	
	pthread_mutex_unlock(&readers_list_lock);

	usec_to_timeval(&wait, max_wait);
	gettimeofday(&start, NULL);
	n = select(max_fd + 1, &read_set, NULL, NULL, &wait);
	if (n < 0) {
		if (errno != EINTR)
			return;
	} else {
		gettimeofday(&stop, NULL);
		timeval_subtract(&dT, &stop, &start);
		elapsed_wait = timeval_to_usec(&dT);

		pthread_mutex_lock(&readers_list_lock);
		list_for_each_entry_safe(ctx, tmp, &readers_list, ctx_reader_head) {
			ctx->elapsed += elapsed_wait;
			if (FD_ISSET(ctx->fd, &read_set)) {
				call_context_handler(ctx, SIOS_EVENT_READ); 
			}
		}
		pthread_mutex_unlock(&readers_list_lock);
	}
}

int sios_source_ctx_exists(struct sios_source_ctx * ctx) 
{
	struct list_head * ptr;

	if (ctx->type & SIOS_POLL_READ) {
		list_for_each(ptr, &readers_list) {
			struct sios_source_ctx * entry;
			entry = container_of(ptr, struct sios_source_ctx, ctx_reader_head);
			if (entry == ctx)
				return 1;
		}

	}

	if (ctx->type & SIOS_POLL_WRITE) {
		list_for_each(ptr, &writers_list) {
			struct sios_source_ctx * entry;
			entry = container_of(ptr, struct sios_source_ctx, ctx_writer_head);
			if (entry == ctx)
				return 1;
		}
	}

	return 0;	
}

int sios_add_source_ctx(struct sios_source_ctx * ctx)
{
	if (sios_source_ctx_exists(ctx)) {
		warn("Source", "source exists (%p)", ctx);
		return -1;
	}
	
	if (ctx->type & SIOS_POLL_READ) {
		pthread_mutex_lock(&readers_list_lock);
		add_source_ctx_unlocked(ctx);
		pthread_mutex_unlock(&readers_list_lock);
	}

	if (ctx->type & SIOS_POLL_WRITE) {
		pthread_mutex_lock(&writers_list_lock);
		add_source_ctx_unlocked(ctx);
		pthread_mutex_unlock(&writers_list_lock);
	}

	return 0;
}

void sios_del_source_ctx(struct sios_source_ctx * ctx)
{
	if (!sios_source_ctx_exists(ctx))
		return;

	if (ctx->type & SIOS_POLL_READ) {
		pthread_mutex_lock(&readers_list_lock);
		del_source_ctx_unlocked(ctx);
		pthread_mutex_unlock(&readers_list_lock);
	}

	if (ctx->type & SIOS_POLL_WRITE) {
		pthread_mutex_lock(&writers_list_lock);
		del_source_ctx_unlocked(ctx);
		pthread_mutex_unlock(&writers_list_lock);
	}
}

void print_sources_list(void)
{
/*
	struct sios_source_ctx * ptr;
	list_for_each_entry(ptr, &sources_list, ctx_head) {
		printf(" %d ", ptr->priority);
	}
	printf("\n");
*/
}
