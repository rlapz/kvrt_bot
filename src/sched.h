#ifndef __SCHED_H__
#define __SCHED_H__


#include <stdatomic.h>
#include <stdint.h>
#include <time.h>

#include <sys/timerfd.h>

#include "ev.h"


typedef struct sched {
	EvCtx       ctx;
	time_t      timeout_s;
	atomic_bool is_ready;
} Sched;

int  sched_init(Sched *s, time_t timeout_s);
void sched_deinit(const Sched *s);


enum {
	SCHED_MESSAGE_TYPE_SEND = 0,
	SCHED_MESSAGE_TYPE_DELETE,

	_SCHED_MESSAGE_TYPES_SIZE,
};

typedef struct sched_param {
	int         type;
	int64_t     chat_id;
	int64_t     message_id;
	int64_t     user_id;
	const char *message;
	time_t      expire;		/* in seconds */
	time_t      interval;		/* in seconds */
} SchedParam;

int sched_add(const SchedParam *param);


#endif
