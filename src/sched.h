#ifndef __SCHED_H__
#define __SCHED_H__


#include <stdatomic.h>

#include <sys/timerfd.h>

#include "ev.h"


typedef struct sched {
	EvCtx       ctx;
	time_t      timeout_s;
	atomic_bool is_ready;
} Sched;

int  sched_init(Sched *s, time_t timeout_s);
void sched_deinit(const Sched *s);


#endif
