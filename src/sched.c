#include <errno.h>
#include <time.h>
#include <unistd.h>

#include <sys/timerfd.h>

#include "sched.h"

#include "common.h"
#include "model.h"
#include "thrd_pool.h"
#include "tg_api.h"
#include "util.h"


static void _spawn_handler(EvCtx *ctx);
static void _handler(void *ctx, void *udata);
static void _run_task(void *ctx, void *udata);
static int  _add(const SchedParam *param, int type);


/*
 * Public
 */
int
sched_init(Sched *s, time_t timeout_s)
{
	const int fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
	if (fd < 0) {
		LOG_ERRP("sched", "%s", "timerfd_create");
		return -1;
	}

	const struct itimerspec timerspec = {
		.it_value = (struct timespec) { .tv_sec = timeout_s },
		.it_interval = (struct timespec) { .tv_sec = timeout_s },
	};

	if (timerfd_settime(fd, 0, &timerspec, NULL) < 0) {
		LOG_ERRP("sched", "%s", "timerfd_settime");
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


int
sched_send_text_plain(const SchedParam *param)
{
	return _add(param, MODEL_SCHED_MESSAGE_TYPE_SEND_TEXT_PLAIN);
}


int
sched_send_text_format(const SchedParam *param)
{
	return _add(param, MODEL_SCHED_MESSAGE_TYPE_SEND_TEXT_FORMAT);
}


int
sched_delete_message(const SchedParam *param)
{
	return _add(param, MODEL_SCHED_MESSAGE_TYPE_DELETE);
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
		LOG_ERRP("sched", "%s", "read");
		return;
	}

	if (rd != sizeof(timer)) {
		LOG_ERRN("sched", "read: invalid size: [%zu:%zu]", rd, sizeof(timer));
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

	const time_t now = time(NULL);
	ModelSchedMessage *msg_list[32];
	int32_t id_list[LEN(msg_list)];

	const int list_len = model_sched_message_get_list(msg_list, LEN(msg_list), now);
	if (list_len <= 0)
		goto out0;

	int count = 0;
	for (; count < list_len; count++) {
		/* run function handler and transfer memory ownership */
		if (thrd_pool_add_job(_run_task, msg_list[count], NULL) < 0)
			goto out1;

		id_list[count] = msg_list[count]->id;
	}

out1:
	model_sched_message_delete(id_list, count);

	/* free() remaining items, in case error was occured */
	for (int j = count; (j < list_len); j++)
		free(msg_list[j]);
out0:
	atomic_store(&s->is_ready, true);
	(void)udata;
}


static void
_run_task(void *ctx, void *udata)
{
	ModelSchedMessage *const msg = (ModelSchedMessage *)ctx;
	const TgMessage tgm = {
		.id = msg->message_id,
		.chat = (TgChat) { .id = msg->chat_id },
		.from = &(TgUser) { .id = msg->user_id },
	};

	int count = 3;
	while (count--) {
		int ret;
		switch (msg->type) {
		case MODEL_SCHED_MESSAGE_TYPE_SEND_TEXT_PLAIN:
			ret = send_text_plain(&tgm, NULL, "%s", msg->value);
			break;
		case MODEL_SCHED_MESSAGE_TYPE_SEND_TEXT_FORMAT:
			ret = send_text_format(&tgm, NULL, "%s", msg->value);
			break;
		case MODEL_SCHED_MESSAGE_TYPE_DELETE:
			ret = delete_message(&tgm);
			break;
		default:
			LOG_ERRN("sched", "%s", "invalid task type");
			ret = 0;
			break;
		}

		if (ret < 0) {
			LOG_ERRN("sched", "error occured! Try again (%d)", count);
			continue;
		}
		
		break;
	}


	free(msg);
	(void)udata;
}


static int
_add(const SchedParam *param, int type)
{
	if (param->chat_id == 0) {
		LOG_ERRN("sched", "%s", "invalid chat_id");
		return -1;
	}

	if (param->user_id == 0) {
		LOG_ERRN("sched", "%s", "invalid user_id");
		return -1;
	}

	if (param->expire < 5) {
		LOG_ERRN("sched", "%s", "invalid expiration time");
		return -1;
	}

	if (param->interval <= 0) {
		LOG_ERRN("sched", "%s", "invalid interval");
		return -1;
	}

	switch (type) {
	case MODEL_SCHED_MESSAGE_TYPE_SEND_TEXT_PLAIN:
	case MODEL_SCHED_MESSAGE_TYPE_SEND_TEXT_FORMAT:
		if (cstr_is_empty(param->message)) {
			LOG_ERRN("sched", "%s", "value is empty");
			return -1;
		}

		break;
	case MODEL_SCHED_MESSAGE_TYPE_DELETE:
		if (param->message_id == 0) {
			LOG_ERRN("sched", "%s", "invalid message_id");
			return -1;
		}

		break;
	default:
		LOG_ERRN("sched", "%s", "model: invalid type");
		return -1;
	}


	const ModelSchedMessage sched = {
		.type = type,
		.chat_id = param->chat_id,
		.message_id = param->message_id,
		.user_id = param->user_id,
		.value_in = param->message,
		.expire = param->expire,
	};

	return model_sched_message_add(&sched, param->interval);
}
