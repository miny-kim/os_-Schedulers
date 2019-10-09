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

#ifndef __SCHED_H__
#define __SCHED_H__

struct scheduler {
	const char *name;

	/***********************************************************************
	 * initialize()
	 *
	 * DESCRIPTION
	 *   Call-back function for your own initialization code. It is OK to
	 *   leave this field NULL if you don't need any initialization.
	 *
	 * RETURN VALUE
	 *   Return 0 on successful initialization.
	 *   Return other value on error, which leads the program to exit.
	 */
	int (*initialize)(void);


	/***********************************************************************
	 * finalize()
	 *
	 * DESCRIPTION
	 *   Callback function for finalizing your code. Like @initialize(),
	 *   you may leave this function NULL.
	 */
	void (*finalize)(void);


	/***********************************************************************
	 * schedule(current_blocked)
	 *
	 * DESCRIPTION
	 *   Schedule the next process. @current points to the current process
	 *   which has been running on the processor.
	 *
	 * RETURN
	 *   process to run next
	 *   NULL if there is no available process to schedule
	 */
	struct process * (*schedule)(bool);


	/***********************************************************************
	 * acquire(resource_id)
	 *
	 * DESCRIPTION
	 *   Callback function to acquire the resource @resource_id.
	 *
	 * RETURN
	 *   true on successful acquision
	 *   false if the resource is already held by others or unavailable
	 */
	bool (*acquire)(int);


	/***********************************************************************
	 * release(resource_id)
	 *
	 * DESCRIPTION
	 *   Callback to release the resource @resource_id
	 *
	 * RETURN
	 *   void
	 */
	void (*release)(int);
};

#endif
