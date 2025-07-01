#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/timerfd.h>

#include "ev.h"

#include "config.h"
#include "util.h"


typedef struct ev {
	atomic_bool is_alive;
	int         fd;
} Ev;


static Ev _instance = { .fd = -1 };


static void _signal_handler(EvCtx *c);
static void _listener_handler(EvCtx *c);
static void _timer_handler(EvCtx *c);


/*
 * Public
 */
int
ev_init(void)
{
	_instance.fd = epoll_create1(0);
	if (_instance.fd < 0)
		return -errno;

	atomic_init(&_instance.is_alive, true);
	return 0;
}


void
ev_deinit(void)
{
	close(_instance.fd);
	_instance.fd = -1;
}


int
ev_run(void)
{
	Event events[CFG_EVENTS_SIZE];
	while (atomic_load_explicit(&_instance.is_alive, memory_order_relaxed)) {
		const int ret = epoll_wait(_instance.fd, events, LEN(events), -1);
		if (ret < 0) {
			if (errno == EINTR)
				continue;

			return -errno;
		}

		for (int i = 0; i < ret; i++) {
			EvCtx *const ctx = (EvCtx *)events[i].data.ptr;
			ctx->callback_fn(ctx);
		}
	}

	return 0;
}


void
ev_stop(void)
{
	atomic_store(&_instance.is_alive, false);
}


bool
ev_is_alive(void)
{
	return atomic_load(&_instance.is_alive);
}


int
ev_ctx_add(EvCtx *c)
{
	c->event.data.ptr = c;
	if (epoll_ctl(_instance.fd, EPOLL_CTL_ADD, c->fd, &c->event) < 0)
		return -errno;

	return 0;
}


int
ev_ctx_mod(EvCtx *c)
{
	if (epoll_ctl(_instance.fd, EPOLL_CTL_MOD, c->fd, &c->event) < 0)
		return -errno;

	return 0;
}


int
ev_ctx_del(EvCtx *c)
{
	if (epoll_ctl(_instance.fd, EPOLL_CTL_DEL, c->fd, &c->event) < 0)
		return -errno;

	return 0;
}


int
ev_signal_init(EvSignal *e, void (*callback_fn)(void *, uint32_t, int), void *udata)
{
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGHUP);
	sigaddset(&mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &mask, NULL);

	const int fd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
	if (fd < 0) {
		log_err(errno, "ev: ev_signal_init: signalfd");
		return -errno;
	}

	*e = (EvSignal) {
		.callback_fn = callback_fn,
		.udata = udata,
		.ctx = (EvCtx) {
			.fd = fd,
			.callback_fn = _signal_handler,
			.event = (Event) {
				.events = EPOLLIN,
			},
		},
	};

	return 0;
}


void
ev_signal_deinit(const EvSignal *e)
{
	close(e->ctx.fd);
}


int
ev_listener_init(EvListener *e, const char host[], uint16_t port, void (*callback_fn)(void *, int), void *udata)
{
	const struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr.s_addr = inet_addr(host),
	};

	const int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
	if (fd < 0) {
		log_err(errno, "ev: ev_listener_init: socket");
		return -1;
	}

	const int y = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(y)) < 0) {
		log_err(errno, "ev: ev_listener_init: setsockopt: SO_REUSEADDR");
		close(fd);
		return -1;
	}

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		log_err(errno, "ev: ev_listener_init: bind");
		close(fd);
		return -1;
	}

	if (listen(fd, 32) < 0) {
		log_err(errno, "ev: ev_listener_init: listen");
		close(fd);
		return -1;
	}

	*e = (EvListener) {
		.callback_fn = callback_fn,
		.udata = udata,
		.ctx = (EvCtx) {
			.fd = fd,
			.callback_fn = _listener_handler,
			.event = (Event) {
				.events = EPOLLIN,
			},
		},
	};

	return 0;
}


void
ev_listener_deinit(const EvListener *e)
{
	close(e->ctx.fd);
}


int
ev_timer_init(EvTimer *e, void (*callback_fn)(void *, int), void *udata, time_t timeout_s)
{
	const int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if (fd < 0) {
		log_err(errno, "ev: ev_timer_init: timerfd_create");
		return -errno;
	}

	const struct itimerspec timerspec = {
		.it_value = (struct timespec) { .tv_sec = timeout_s },
		.it_interval = (struct timespec) { .tv_sec = timeout_s },
	};

	if (timerfd_settime(fd, 0, &timerspec, NULL) < 0) {
		const int ret = -errno;
		log_err(ret, "ev: ev_timer_init: timerfd_settime");
		close(fd);
		return ret;
	}

	*e = (EvTimer) {
		.callback_fn = callback_fn,
		.udata = udata,
		.timeout_s = timeout_s,
		.ctx = (EvCtx) {
			.fd = fd,
			.callback_fn = _timer_handler,
			.event = (Event) {
				.events = EPOLLIN,
			},
		},
	};

	return 0;
}


void
ev_timer_deinit(const EvTimer *e)
{
	close(e->ctx.fd);
}


/*
 * Private
 */
static void
_signal_handler(EvCtx *c)
{
	EvSignal *const s = FIELD_PARENT_PTR(EvSignal, ctx, c);
	struct signalfd_siginfo siginfo;

	const ssize_t ret = read(s->ctx.fd, &siginfo, sizeof(siginfo));
	if (ret <= 0) {
		s->callback_fn(s->udata, 0, errno);
		return;
	}

	switch (siginfo.ssi_signo) {
	case SIGINT:
		s->callback_fn(s->udata, siginfo.ssi_signo, 0);
		break;
	case SIGHUP:
	case SIGCHLD:
		break;
	default:
		/* TODO */
		exit(1);
	}
}


static void
_listener_handler(EvCtx *c)
{
	EvListener *const l = FIELD_PARENT_PTR(EvListener, ctx, c);
	int ret = accept(c->fd, NULL, NULL);
	if (ret < 0)
		ret = -errno;

	l->callback_fn(l->udata, ret);
}


static void
_timer_handler(EvCtx *c)
{
	EvTimer *const t = FIELD_PARENT_PTR(EvTimer, ctx, c);
	uint64_t timer = 0;

	int err = 0;
	if (read(t->ctx.fd, &timer, sizeof(timer)) < 0)
		err = errno;

	t->callback_fn(t->udata, err);
}
