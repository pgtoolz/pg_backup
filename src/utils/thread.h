/*-------------------------------------------------------------------------
 *
 * thread.h: - pthread implementations.
 *
 * Copyright (c) 2018-2019, Postgres Professional
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROBACKUP_THREAD_H
#define PROBACKUP_THREAD_H

/* Use platform-dependent pthread capability */
#include <pthread.h>

extern pthread_t main_tid;

extern bool			thread_interrupted;

#endif   /* PROBACKUP_THREAD_H */
