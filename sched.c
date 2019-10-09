/**********************************************************************
 * Copyright (c) 2019
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

/* THIS FILE IS ALL YOURS; DO WHATEVER YOU WANT TO DO IN THIS FILE */

#include <stdio.h>
#include <assert.h>

#include "types.h"
#include "list_head.h"

/**
 * The process which is currently running
 */
#include "process.h"
extern struct process *current;

/**
 * List head to hold the processes ready to run
 */
extern struct list_head readyqueue;


/**
 * Resources in the system.
 */
#include "resource.h"
extern struct resource resources[NR_RESOURCES];


/**
 * Monotonically increasing ticks
 */
extern unsigned int ticks;


/***********************************************************************
 * Default resource acquision function
 *
 * DESCRIPTION
 *   This is the default resource acquision function which is called back
 *   whenever the current process is to acquire resource @resource_id.
 *   See the comments in sched.h
 ***********************************************************************/
bool default_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		return true;
	}

	/* OK, this resource is taken by @r->owner. Add current to waitqueue */
	list_add(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule the next process to run.
	 */
	return false;
}

/***********************************************************************
 * Default resource release function
 *
 * DESCRIPTION
 *   This is the default resource release function which is called back
 *   whenever the current process is to release resource @resource_id.
 *   See the comments in sched.h
 ***********************************************************************/
void default_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);

	/* Un-own this resource */
	r->owner = NULL;

	/* Let's wake up ONE waiter if exists */
	if (!list_empty(&r->waitqueue)) {
		struct process *p = list_first_entry(&r->waitqueue, struct process, list);

		/* Take out from the waiting queue. Note we use list_del_init() instead
		 * of list_del() to maintain the list head sane otherwise the framework
		 * will complain on process exit. */
		list_del_init(&p->list);

		/* Put the process into ready queue. The framework will do the rest */
		list_add_tail(&p->list, &readyqueue);
	}
}



#include "sched.h"

/***********************************************************************
 * FIFO scheduler
 ***********************************************************************/
static int fifo_initialize(void)
{
	return 0;
}

static void fifo_finalize(void)
{
}

static struct process *fifo_schedule(bool current_blocked)
{
	struct process *next = NULL;

	/**
	 * In the beginning, there will be no current process. In this case,
	 * pick the next without examining the current process. Also, when
	 * the current process is blocked while acquiring a resource,
	 * @current is (supposed to be) attached to the waitqueue of
	 * the corresponding resource. In this case just pick the next.
	 */
	if (!current || current_blocked) {
		goto pick_next;
	}

	/* The current process has remaining lifetime. Schedule it again */
	if (current->age < current->lifespan) {
		return current;
	}

pick_next:
	/* Let's pick a new process to run next */

	if (!list_empty(&readyqueue)) {
		/**
		 * If the ready queue is not empty, pick the first process
		 * in the ready queue
		 */
		next = list_first_entry(&readyqueue, struct process, list);

		/* Detach the process from the ready queue. Note we use list_del_init()
		 * instead of list_del() to maintain the list head sane. Otherwise,
		 * the framework will complain (assert) on process exit. */
		list_del_init(&next->list);
	}

	/* Return the next process to run */
	return next;
}

struct scheduler fifo_scheduler = {
	.name = "fifo",
	.acquire = default_acquire,
	.release = default_release,
	.initialize = fifo_initialize,
	.finalize = fifo_finalize,
	.schedule = fifo_schedule,
};



/***********************************************************************
 * SJF scheduler
 ***********************************************************************/
static struct process *sjf_schedule(bool current_blocked)
{
	return NULL;
}

struct scheduler sjf_scheduler = {
	.name = "SJF",
	.acquire = default_acquire,
	.release = default_release,
	.schedule = sjf_schedule,
};


/***********************************************************************
 * Round-robin scheduler
 ***********************************************************************/
struct scheduler rr_scheduler = {
	.name = "Round-robin",
	.acquire = default_acquire,
	.release = default_release,
};


/***********************************************************************
 * Priority scheduler
 ***********************************************************************/
struct scheduler prio_scheduler = {
	.name = "Priority",
};


/***********************************************************************
 * Priority scheduler with priority inheritance protocol
 ***********************************************************************/
struct scheduler pip_scheduler = {
	.name = "Priority + priority inheritance protocol",
};
