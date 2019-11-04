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
#include <stdlib.h>
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


/**
 * Quiet mode. True if the program was started with -q option
 */
extern bool quiet;


/***********************************************************************
 * Default FCFS resource acquision function
 *
 * DESCRIPTION
 *   This is the default resource acquision function which is called back
 *   whenever the current process is to acquire resource @resource_id.
 *   The current implementation serves the resource in the requesting order
 *   without considering the priority. See the comments in sched.h
 ***********************************************************************/
bool fcfs_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		return true;
	}

	/* OK, this resource is taken by @r->owner. */

	/* Update the current process state */
	current->status = PROCESS_WAIT;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	return false;
}

/***********************************************************************
 * Default FCFS resource release function
 *
 * DESCRIPTION
 *   This is the default resource release function which is called back
 *   whenever the current process is to release resource @resource_id.
 *   The current implementation serves the resource in the requesting order
 *   without considering the priority. See the comments in sched.h
 ***********************************************************************/
void fcfs_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);

	/* Un-own this resource */
	r->owner = NULL;

	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue)) {
		struct process *waiter =
				list_first_entry(&r->waitqueue, struct process, list);

		/**
		 * Ensure the waiter  is in the wait status
		 */
		assert(waiter->status == PROCESS_WAIT);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);

		/* Update the process status */
		waiter->status = PROCESS_READY;

		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
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

static struct process *fifo_schedule(void)
{
	struct process *next = NULL;

	/* You may inspect the situation by calling dump_status() at any time */
	// dump_status();

	/**
	 * When there was no process to run in the previous tick (so does
	 * in the very beginning of the simulation), there will be
	 * no @current process. In this case, pick the next without examining
	 * the current process. Also, when the current process is blocked
	 * while acquiring a resource, @current is (supposed to be) attached
	 * to the waitqueue of the corresponding resource. In this case just
	 * pick the next as well.
	 */
	if (!current || current->status == PROCESS_WAIT) {
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

		/**
		 * Detach the process from the ready queue. Note we use list_del_init()
		 * instead of list_del() to maintain the list head tidy. Otherwise,
		 * the framework will complain (assert) on process exit.
		 */
		list_del_init(&next->list);
	}

	/* Return the next process to run */
	return next;
}


struct scheduler fifo_scheduler = {
	.name = "FIFO",
	.acquire = fcfs_acquire,
	.release = fcfs_release,
	.initialize = fifo_initialize,
	.finalize = fifo_finalize,
	.schedule = fifo_schedule,
};


/***********************************************************************
 * SJF scheduler
 ***********************************************************************/
static struct process *sjf_schedule(void)
{
    struct process *next = NULL;
    struct process *pnext = NULL;
    struct process *endp = NULL;
    int min = 100;

   //현재 상태가 wait일때
    if(!current || current->status == PROCESS_WAIT){
        goto pick_next;
    }//next 결정해서 뽑기

    if(current->age < current->lifespan){
        return current;}

pick_next:
    
if(!list_empty(&readyqueue)){
    pnext = list_first_entry(&readyqueue, struct process, list);
    endp = list_last_entry(&readyqueue, struct process, list);
    printf("endp->pid : %d\n", endp->pid);
    next = pnext;
   // max = pnext->prio;
   // next = pnext;
   // pnext = list_next_entry(pnext, list);

    while(pnext){
       
        if(min>pnext->lifespan){
            min = pnext->lifespan;
            next = pnext;
        }
        if(pnext->pid == endp->pid)
            break;
        pnext = list_next_entry(pnext, list);
        //if(pnext->pid == 0)
          //  break;
    }

    list_del_init(&next->list);
}
	return next;

}

struct scheduler sjf_scheduler = {
	.name = "Shortest-Job First",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	.schedule = sjf_schedule,		 /* TODO: Assign sjf_schedule()
								to this function pointer to activate
								SJF in the system */
};


/***********************************************************************
 * SRTF scheduler
 ***********************************************************************/

static struct process * srtf_schedule(void)
{
    struct process *next = NULL;
    struct process *pnext = NULL;
    struct process *endp = NULL;
    int min = 100;
    int remain = 0;
   //현재 상태가 wait일때
    if(!current || current->status == PROCESS_WAIT){
        goto pick_next;
    }//next 결정해서 뽑기

    if(current->age < current->lifespan){
        list_add_tail(&current->list, &readyqueue);
        goto pick_next;
    }

pick_next:
    
if(!list_empty(&readyqueue)){
    pnext = list_first_entry(&readyqueue, struct process, list);
    endp = list_last_entry(&readyqueue, struct process, list);
   // printf("endp->pid : %d\n", endp->pid);
    next = pnext;
   // max = pnext->prio;
   // next = pnext;
   // pnext = list_next_entry(pnext, list);

    while(pnext){
       remain = (pnext->lifespan)-(pnext->age);
       printf("pid,remain : %d, %d\n", pnext->pid, remain);

        if(min>remain){
            min = remain;
            next = pnext;
        }
        if(pnext->pid == endp->pid)
            break;
        pnext = list_next_entry(pnext, list);
        //if(pnext->pid == 0)
          //  break;
    }

    list_del_init(&next->list);
}
	return next;


}

struct scheduler srtf_scheduler = {
	.name = "Shortest Remaining Time First",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	.schedule = srtf_schedule,
    /* You need to check the newly created processes to implement SRTF.
	 * Use @forked() callback to mark newly created processes */
	/* Obviously, you should implement srtf_schedule() and attach it here */
};


/***********************************************************************
 * Round-robin scheduler
 ***********************************************************************/
static struct process * rr_schedule(void)
{
    struct process *next = NULL;
    struct process *endp = NULL;

   //현재 상태가 wait일때
    if(!current || current->status == PROCESS_WAIT){
        goto pick_next;
    }//next 결정해서 뽑기

    //quantum 적용하기
    if(current->age < current->lifespan){
        list_add_tail(&current->list , &readyqueue);
        goto pick_next;
    }

pick_next:
	if (!list_empty(&readyqueue)) {
		/**
		 * If the ready queue is not empty, pick the first process
		 * in the ready queue
		 */
		next = list_first_entry(&readyqueue, struct process, list);

		/**
		 * Detach the process from the ready queue. Note we use list_del_init()
		 * instead of list_del() to maintain the list head tidy. Otherwise,
		 * the framework will complain (assert) on process exit.
		 */
		list_del_init(&next->list);
	}

	/* Return the next process to run */
	return next;

}

struct scheduler rr_scheduler = {
	.name = "Round-Robin",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
    .schedule = rr_schedule,	/* Obviously, you should implement rr_schedule() and attach it here */
};


/***********************************************************************
 * Priority scheduler
 ***********************************************************************/

bool prio_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		return true;
	}

	/* OK, this resource is taken by @r->owner. */

	/* Update the current process state */
	current->status = PROCESS_WAIT;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	return false;
}

void prio_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);
   // printf("owner : %d\n", r->owner->pid);
	/* Un-own this resource */
	r->owner = NULL;


	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue)) {
		struct process *waiter =
				list_first_entry(&r->waitqueue, struct process, list);
        struct process * pwaiter = waiter;
        struct process * endp = list_last_entry(&r->waitqueue, struct process, list);
     //   printf("endp->pid : %d\n", endp->pid);
        int max = 0;
        
        while(pwaiter){
           // printf("first : %d\n", pwaiter->pid);
            if(max<pwaiter->prio){
                max = pwaiter->prio;
                waiter = pwaiter;
             //   printf("final : %d\n", waiter->pid);
            }

            if(pwaiter->pid == endp->pid)
                break;
            pwaiter = list_next_entry(pwaiter, list);
            // printf("inside while (pwaiter->pid):%d\n", pwaiter->pid); 
        }
       // printf("outside while (waiter->pid) : %d\n", waiter->pid);
		/**
		 * Ensure the waiter  is in the wait status
		 */
		assert(waiter->status == PROCESS_WAIT);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);

		/* Update the process status */
		waiter->status = PROCESS_READY;

		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
	}
}

static struct process * prio_schedule(void)
{
    struct process * next = NULL;
    struct process * pnext = NULL;
    struct process * endp = NULL;
    int max = 0;

    if(!current || current->status == PROCESS_WAIT){
        goto pick_next;
    }

    if(current->age < current->lifespan){
        list_add_tail(&current->list, &readyqueue);
        goto pick_next;
    }

pick_next:

    if(!list_empty(&readyqueue)){
        pnext = list_first_entry(&readyqueue, struct process, list);
        endp = list_last_entry(&readyqueue, struct process, list);
        next = pnext;
   // max = pnext->prio;
   // next = pnext;
   // pnext = list_next_entry(pnext, list);

        while(pnext){
       
            if(max<pnext->prio){
                max = pnext->prio;
                next = pnext;
            }
            if(pnext->pid == endp->pid)
                break;
            pnext = list_next_entry(pnext, list);
            //if(pnext->pid == 0)
              //  break;
        }

        list_del_init(&next->list);
    }
	return next;

}

struct scheduler prio_scheduler = {
	.name = "Priority",
    .acquire = prio_acquire,
    .release = prio_release,
    .schedule = prio_schedule,
	/**
	 * Implement your own acqure/release function to make priority
	 * scheduler correct.
	 */
	/* Implement your own prio_schedule() and attach it here */
};


/***********************************************************************
 * Priority scheduler with priority inheritance protocol
 ***********************************************************************/
struct scheduler pip_scheduler = {
	.name = "Priority + Priority Inheritance Protocol",
	/**
	 * Implement your own acqure/release function too to make priority
	 * scheduler correct.
	 */
	/* It goes without saying to implement your own pip_schedule() */
};
