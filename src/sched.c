#include <errno.h>
#include <time.h>
#include <unistd.h>

#include <sys/timerfd.h>

#include "sched.h"

#include "model.h"
#include "thrd_pool.h"
#include "tg_api.h"
#include "util.h"


static void _spawn_handler(EvCtx *ctx);
static void _handler(void *ctx, void *udata);
static void _run_task(void *ctx, void *udata);


/*
 * Public
 */
int
sched_init(Sched *s, time_t timeout_s)
{
	const int fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
	if (fd < 0) {
		log_err(errno, "sched: sched_init: timerfd_create");
		return -1;
	}

	const struct itimerspec timerspec = {
		.it_value = (struct timespec) { .tv_sec = timeout_s },
		.it_interval = (struct timespec) { .tv_sec = timeout_s },
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
_spawn_handler(EvCtx *ctx)
{
	Sched *const s = FIELD_PARENT_PTR(Sched, ctx, ctx);

	uint64_t timer = 0;
	const ssize_t rd = read(ctx->fd, &timer, sizeof(timer));
	if (rd < 0) {
		log_err(errno, "sched: _spawn_handler: read");
		return;
	}

	if (rd != sizeof(timer)) {
		log_err(errno, "sched: _spawn_handler: read: invalid size: [%zu:%zu]", rd, sizeof(timer));
		return;
	}

	/* only 1 thread could call _handler */
	bool expected = true;
	if (atomic_compare_exchange_strong(&s->is_ready, &expected, false))
		thrd_pool_add_job(_handler, s, NULL);
}


static void
_handler(void *ctx, void *udata)
{
	Sched *const s = (Sched *)ctx;

	int is_err = 1;
	const time_t now = time(NULL);
	ModelSchedMessage *msg_list[32];
	int32_t id_list[LEN(msg_list)];

	const int list_len = model_sched_message_get_list(msg_list, LEN(msg_list), now);
	if (list_len <= 0)
		goto out0;

	int count = 0;
	for (; count < list_len; count++) {
		if (thrd_pool_add_job(_run_task, msg_list[count], NULL) < 0)
			goto out1;

		id_list[count] = msg_list[count]->id;
	}

	is_err = 0;

out1:
	model_sched_message_delete(id_list, count);
	for (int j = 0; (j < count) && (is_err); j++)
		free(msg_list[j]);
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
		tg_api_send_text(TG_API_TEXT_TYPE_FORMAT, msg->chat_id, msg->message_id, msg->value, NULL);
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
