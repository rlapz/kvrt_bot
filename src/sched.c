#include <errno.h>
#include <time.h>
#include <unistd.h>

#include <sys/timerfd.h>

#include "sched.h"

#include "model.h"
#include "thrd_pool.h"
#include "tg_api.h"
#include "util.h"


static void _reset(Sched *s);
static void _spawn_handler(EvCtx *ctx);
static void _handler(void *ctx, void *udata);
static void _run_task(void *ctx, void *udata);


/*
 * Public
 */
int
sched_init(Sched *s, time_t timeout_s)
{
	const int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if (fd < 0) {
		log_err(errno, "sched: sched_init: timerfd_create");
		return -1;
	}

	const struct itimerspec timerspec = {
		.it_value = (struct timespec) {
			.tv_sec = timeout_s,
		},
	};

	if (timerfd_settime(fd, 0, &timerspec, NULL) < 0) {
		log_err(errno, "sched: sched_init: timerfd_settime");
		close(fd);
		return -1;
	}

	*s = (Sched) {
		.ctx = (EvCtx) {
			.fd = fd,
			.callback_fn = _spawn_handler,
			.event = (Event) {
				.events = EPOLLIN,
			},
		},
		.is_ready = true,
		.timeout_s = timeout_s,
	};

	return 0;
}


void
sched_deinit(const Sched *s)
{
	close(s->ctx.fd);
}


/*
 * Private
 */
static void
_reset(Sched *s)
{
	const struct itimerspec timerspec = {
		.it_value = (struct timespec) {
			.tv_sec = s->timeout_s,
		},
	};

	if (timerfd_settime(s->ctx.fd, 0, &timerspec, NULL) < 0)
		log_err(errno, "sched: _reset: timerfd_settime");
}


static void
_spawn_handler(EvCtx *ctx)
{
	Sched *const s = FIELD_PARENT_PTR(Sched, ctx, ctx);

	uint64_t timer = 0;
	const ssize_t rd = read(ctx->fd, &timer, sizeof(timer));
	if (rd < 0) {
		log_err(errno, "sched: _spawn_handler: read");
		goto out0;
	}

	if (rd != sizeof(timer)) {
		log_err(errno, "sched: _spawn_handler: read: invalid size: [%zu:%zu]", rd, sizeof(timer));
		goto out0;
	}

	/* only 1 thread could call _handler */
	bool expected = true;
	bool desired = false;
	if (atomic_compare_exchange_strong(&s->is_ready, &expected, desired))
		thrd_pool_add_job(_handler, s, NULL);

out0:
	_reset(s);
}


static void
_handler(void *ctx, void *udata)
{
	Sched *const s = (Sched *)ctx;
	const time_t now = time(NULL);
	ModelSchedMessage **msg_list = NULL;
	const int list_len = model_sched_message_get_list(now, &msg_list);
	if (list_len <= 0)
		goto out0;

	int i = 0;
	for (; i < list_len; i++) {
		if (thrd_pool_add_job(_run_task, msg_list[i], NULL) < 0)
			goto out1;
	}

	model_sched_message_del_all(now);
	i = 0;

out1:
	for (int j = 0; j < i; j++)
		free(msg_list[j]);

	free(msg_list);
out0:
	atomic_store(&s->is_ready, true);
	(void)udata;
}


static void
_run_task(void *ctx, void *udata)
{
	ModelSchedMessage *const msg = (ModelSchedMessage *)ctx;
	switch (msg->type) {
	case MODEL_SCHED_MESSAGE_TYPE_SEND:
		tg_api_send_text(TG_API_TEXT_TYPE_FORMAT, msg->chat_id, &msg->message_id, msg->value_out, NULL);
		break;
	case MODEL_SCHED_MESSAGE_TYPE_DELETE:
		tg_api_delete_message(msg->chat_id, msg->message_id);
		break;
	default:
		log_err(0, "sched: _run_task: invalid task type");
		break;
	}

	free(msg);
	(void)udata;
}
