#ifndef __EV_H__
#define __EV_H__


#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include <sys/epoll.h>


typedef struct epoll_event Event;

typedef struct ev_ctx {
	int   fd;
	Event event;
	void  (*callback_fn)(struct ev_ctx *c);
} EvCtx;

int  ev_init(void);
void ev_deinit(void);
int  ev_run(void);
void ev_stop(void);
bool ev_is_alive(void);
int  ev_ctx_add(EvCtx *c);
int  ev_ctx_mod(EvCtx *c);
int  ev_ctx_del(EvCtx *c);


typedef struct ev_signal {
	EvCtx  ctx;
	void   (*callback_fn)(void *udata, uint32_t signo, int err);
	void  *udata;
} EvSignal;

int  ev_signal_init(EvSignal *e, void (*callback_fn)(void *, uint32_t, int), void *udata);
void ev_signal_deinit(const EvSignal *e);


typedef struct ev_listener {
	EvCtx  ctx;
	void   (*callback_fn)(void *udata, int fd);
	void  *udata;
	int    fd;
} EvListener;

int  ev_listener_init(EvListener *e, const char host[], uint16_t port, void (*callback_fn)(void *, int),
		      void *udata);
void ev_listener_deinit(const EvListener *e);


typedef struct ev_timer {
	EvCtx   ctx;
	time_t  timeout_s;
	void    (*callback_fn)(void *udata, int err);
	void   *udata;
} EvTimer;

int  ev_timer_init(EvTimer *e, void (*callback_fn)(void *, int), void *udata, time_t timeout_s);
void ev_timer_deinit(const EvTimer *e);


#endif
