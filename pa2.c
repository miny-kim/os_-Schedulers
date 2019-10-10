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

/*====================================================================*/
/*          ******        DO NOT MODIFY THIS FILE        ******       */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>

#include "types.h"
#include "list_head.h"

#include "parser.h"
#include "process.h"
#include "resource.h"

#include "sched.h"

/**
 * List head to hold the processes ready to run
 */
LIST_HEAD(readyqueue);

/**
 * The process that is currently running
 */
struct process *current = NULL;

/**
 * Number of generated ticks since the simulator was started
 */
unsigned int ticks = 0;

/**
 * Resources in the system.
 */
struct resource resources[NR_RESOURCES];

/**
 * Following code is to maintain the simulator itself.
 */
struct resource_schedule {
	int resource_id;
	int at;
	int duration;
	struct list_head list;
};

static LIST_HEAD(__forkqueue);

static bool __quiet = false;

/**
 * Assorted schedulers
 */
extern struct scheduler fifo_scheduler;
extern struct scheduler sjf_scheduler;
extern struct scheduler rr_scheduler;
extern struct scheduler prio_scheduler;
extern struct scheduler pip_scheduler;

static struct scheduler *sched = &fifo_scheduler;

void dump_status(void)
{
	struct process *p;

	printf("***** CURRENT *********\n");
	if (current) {
		printf("%2d: %d + %d/%d at %d\n",
				current->pid, current->__starts_at,
				current->age, current->lifespan, current->prio);
	}

	printf("***** READY QUEUE *****\n");
	list_for_each_entry(p, &readyqueue, list) {
		printf("%2d: %d + %d/%d at %d\n",
				p->pid, p->__starts_at, p->age, p->lifespan, p->prio);
	}

	printf("***** RESOURCES *******\n");
	for (int i = 0; i < NR_RESOURCES; i++) {
		struct resource *r = resources + i;;
		if (r->owner || !list_empty(&r->waitqueue)) {
			printf("%2d: owned by ", i);
			if (r->owner) {
				printf("%d\n", i, r->owner->pid);
			} else {
				printf("no one\n");
			}

			list_for_each_entry(p, &r->waitqueue, list) {
				printf("    %d is waiting\n", p->pid);
			}
		}
	}
	printf("\n\n");

	return;
}

#define __print_event(pid, string, args...) do { \
	fprintf(stderr, "%3d: ", ticks); \
	for (int i = 0; i < pid; i++) { \
		fprintf(stderr, "    "); \
	} \
	fprintf(stderr, string "\n", ##args); \
} while (0);

static inline bool strmatch(char * const str, const char *expect)
{
	return (strlen(str) == strlen(expect)) && (strncmp(str, expect, strlen(expect)) == 0);
}

static void __briefing_process(struct process *p)
{
	struct resource_schedule *rs;

	if (__quiet) return;

	printf("- Process %d: Forked at tick %d and run for %d tick%s with initial priority %d\n",
				p->pid, p->__starts_at, p->lifespan,
				p->lifespan >= 2 ? "s" : "", p->prio);

	list_for_each_entry(rs, &p->__resources_to_acquire, list) {
		printf("    Acquire resource %d at %d for %d\n", rs->resource_id, rs->at, rs->duration);
	}
}

static int __load_script(char * const filename)
{
	char line[256];
	struct process *p = NULL;

	FILE *file = fopen(filename, "r");
	while (fgets(line, sizeof(line), file)) {
		char *tokens[32] = { NULL };
		int nr_tokens;

		parse_command(line, &nr_tokens, tokens);

		if (nr_tokens == 0) continue;

		if (strmatch(tokens[0], "process")) {
			assert(nr_tokens == 2);
			/* Start processor description */
			p = malloc(sizeof(*p));
			memset(p, 0x00, sizeof(*p));

			p->pid = atoi(tokens[1]);

			INIT_LIST_HEAD(&p->list);
			INIT_LIST_HEAD(&p->__resources_to_acquire);
			INIT_LIST_HEAD(&p->__resources_holding);

			continue;
		} else if (strmatch(tokens[0], "end")) {
			/* End of process description */
			struct resource_schedule *rs;
			assert(p);

			list_add_tail(&p->list, &__forkqueue);

			__briefing_process(p);
			p = NULL;

			continue;
		}

		if (strmatch(tokens[0], "lifespan")) {
			assert(nr_tokens == 2);
			p->lifespan = atoi(tokens[1]);
		} else if (strmatch(tokens[0], "prio")) {
			assert(nr_tokens == 2);
			p->prio = p->prio_orig = atoi(tokens[1]);
		} else if (strmatch(tokens[0], "start")) {
			assert(nr_tokens == 2);
			p->__starts_at = atoi(tokens[1]);
		} else if (strmatch(tokens[0], "acquire")) {
			struct resource_schedule *rs;
			assert(nr_tokens == 4);

			rs = malloc(sizeof(*rs));

			rs->resource_id = atoi(tokens[1]);
			rs->at = atoi(tokens[2]);
			rs->duration = atoi(tokens[3]);

			list_add_tail(&rs->list, &p->__resources_to_acquire);
		} else {
			fprintf(stderr, "Unknown property %s\n", tokens[0]);
			return false;
		}
	}
	fclose(file);
	if (!__quiet) printf("\n");
	return true;
}

/**
 * Fork process on schedule
 */
static void __fork_on_schedule()
{
	struct process *p, *tmp;
	list_for_each_entry_safe(p, tmp, &__forkqueue, list) {
		if (p->__starts_at <= ticks) {
			list_move_tail(&p->list, &readyqueue);
			p->status = PROCESS_READY;
			__print_event(p->pid, "N");
		}
	}
}

/**
 * Process resource acqutision
 */
static bool __run_current_acquire()
{
	struct resource_schedule *rs, *tmp;

	list_for_each_entry_safe(rs, tmp, &current->__resources_to_acquire, list) {
		if (rs->at == current->age) {
			assert(sched->acquire && "scheduler.acquire() not implemented");

			/* Callback to acquire the resource */
			if (sched->acquire(rs->resource_id)) {
				list_move_tail(&rs->list, &current->__resources_holding);

				__print_event(current->pid, "+%d", rs->resource_id);
			} else {
				return true;
			}
		}
	}

	return false;
}

/**
 * Process resource release
 */
static void __run_current_release()
{
	struct resource_schedule *rs, *tmp;

	list_for_each_entry_safe(rs, tmp, &current->__resources_holding, list) {
		if (--rs->duration == 0) {
			assert(sched->release && "scheduler.release() not implemented");

			/* Callback the release() */
			sched->release(rs->resource_id);

			__print_event(current->pid, "-%d", rs->resource_id);

			list_del(&rs->list);
			free(rs);
		}
	}
}


/**
 * Exit the process
 */
static void __exit_process(struct process *p)
{
	/* Make sure the process is not attached to some list head */
	assert(list_empty(&p->list));

	/* Make sure the process is not holding any resource */
	assert(list_empty(&p->__resources_holding));

	/* Make sure there is no pending resource to acquire */
	assert(list_empty(&p->__resources_to_acquire));

	__print_event(p->pid, "X");

	free(p);
}


/****
 * The main loop for the simulation
 */
static void __do_simulation(void)
{
	bool blocked = false;

	assert(sched->schedule && "scheduler.schedule() not implemented");

	while (true) {
		struct process *prev;

		/* Fork processes on schedule */
		__fork_on_schedule();

		/* Ask scheduler to pick the next process to run */
		prev = current;
		current = sched->schedule(blocked);

		/* Decommission the completed process */
		if (prev) {
			if (prev->age == prev->lifespan) {
				__exit_process(prev);
			}
		}

		/* No process is ready to run at this moment */
		if (!current) {
			/* Quit simulation if no pending process exists */
			if (list_empty(&readyqueue) && list_empty(&__forkqueue)) {
				break;
			}

			/* Idle temporarily */
			fprintf(stderr, "%3d: idle\n", ticks);
			goto next;
		}

		/* Execute the current process */
		if ((blocked = __run_current_acquire())) {
			/**
			 * The current is blocked while acquiring resource(s). In this case,
			 * It did not make a progress in this tick
			 */
			__print_event(current->pid, "=");
		} else {
			/**
			 * The current acquired all the resources to make a progress.
			 * So, it ages by one tick and performs pending releases
			 */
			__print_event(current->pid, "%d", current->pid);

			current->age++;
			__run_current_release();
		}

next:
		ticks++;
	}
}


static void __initialize(void)
{
	INIT_LIST_HEAD(&readyqueue);

	for (int i = 0; i < NR_RESOURCES; i++) {
		resources[i].owner = NULL;
		INIT_LIST_HEAD(&(resources[i].waitqueue));
	}

	INIT_LIST_HEAD(&__forkqueue);

	if (__quiet) return;
	printf("**************************************************************\n");
	printf("*\n");
	printf("*   Simulating %s scheduler\n", sched->name);
	printf("*\n");
	printf("**************************************************************\n");
	printf("   N: Forked\n");
	printf("   X: Finished\n");
	printf("   =: Blocked\n");
	printf("  +n: Acquire resource n\n");
	printf("  -n: Release resource n\n");
	printf("\n");
}


static void __print_usage(char * const name)
{
	printf("Usage: %s {-q} -[f|s|r|p|i] [process script file]\n", name);
	printf("\n");
	printf("  -q: Run quietly\n\n");
	printf("  -f: Use FIFO scheduler (default)\n");
	printf("  -s: Use SJF scheduler\n");
	printf("  -r: Use Round-robin scheduler\n");
	printf("  -p: Use Priority scheduler\n");
	printf("  -i: Use Priority with PIP scheduler\n\n");
}


int main(int argc, char * const argv[])
{
	int opt;
	char *scriptfile;

	while ((opt = getopt(argc, argv, "qfsrpih")) != -1) {
		switch (opt) {
		case 'q':
			__quiet = true;
			break;

		case 'f':
			sched = &fifo_scheduler;
			break;
		case 's':
			sched = &sjf_scheduler;
			break;
		case 'r':
			sched = &rr_scheduler;
			break;
		case 'p':
			sched = &prio_scheduler;
			break;
		case 'i':
			sched = &pip_scheduler;
			break;
		case 'h':
		default:
			__print_usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (optind >= argc) {
		__print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	scriptfile = argv[optind];

	__initialize();

	if (!__load_script(scriptfile)) {
		return EXIT_FAILURE;
	}

	if (sched->initialize && sched->initialize()) {
		return EXIT_FAILURE;
	}

	__do_simulation();

	if (sched->finalize) {
		sched->finalize();
	}

	return EXIT_SUCCESS;
}
/*          ******        DO NOT MODIFY THIS FILE        ******       */
/*====================================================================*/
