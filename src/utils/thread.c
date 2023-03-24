/*-------------------------------------------------------------------------
 *
 * thread.c: - multi-platform pthread implementations.
 *
 * Copyright (c) 2018-2019, Postgres Professional
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"

#include "thread.h"

/*
 * Global var used to detect error condition (not signal interrupt!) in threads,
 * so if one thread errored out, then others may abort
 */
bool thread_interrupted = false;

pthread_t main_tid = 0;

