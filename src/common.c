#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"

#include "config.h"
#include "model.h"
#include "tg_api.h"
#include "util.h"


static int  _exec_with_deleter(TgApi *t, int64_t *ret_id);
static void _send_error(int64_t chat_id, int64_t msg_id, int64_t user_id, const TgApi *api, int err);
static int  _add_deleter(TgApi *t);

static char *_pager_add_template(const Pager *p, const PagerList *list);


/*
 * tg_api wrappers
 */
int
send_text(const TgMessage *msg, int parse_type, const char text[], int64_t *ret_id)
{
	TgApi api = {
		.type = TG_API_TYPE_SEND_TEXT,
		.parse_type = parse_type,
		.chat_id = msg->chat.id,
		.msg_id = msg->id,
		.user_id = msg->from->id,
		.value = text,
	};

	return _exec_with_deleter(&api, ret_id);
}


int
send_photo(const TgMessage *msg, int parse_type, const char photo[], const char caption[], int64_t *ret_id)
{
	TgApi api = {
		.type = TG_API_TYPE_SEND_PHOTO,
		.parse_type = parse_type,
		.value_type = TG_API_VALUE_TYPE_URL,
		.chat_id = msg->chat.id,
		.msg_id = msg->id,
		.user_id = msg->from->id,
		.value = photo,
		.caption = caption,
	};

	return _exec_with_deleter(&api, ret_id);
}


int
answer_callback(int value_type, const char id[], const char value[], int show_alert)
{
	int ret = -1;
	TgApi api = {
		.type = TG_API_TYPE_ANSWER_CALLBACK,
		.value_type = value_type,
		.callback_id = id,
		.value = value,
		.callback_show_alert = show_alert,
	};

	int res = tg_api_init(&api);
	if (res != TG_API_ERR_NONE) {
		LOG_ERRN("common", "tg_api_init: %s", tg_api_err_str(res));
		goto out0;
	}

	res = tg_api_exec(&api);
	if (res != TG_API_ERR_NONE) {
		LOG_ERRN("common", "tg_api_exec: %s", tg_api_err_str(res));
		goto out0;
	}

	ret = 0;

out0:
	if (ret < 0)
		_send_error(api.chat_id, api.msg_id, api.user_id, &api, res);

	tg_api_deinit(&api);
	return ret;
}


int
delete_message(int64_t chat_id, int64_t message_id)
{
	int ret = -1;
	TgApi api = {
		.type = TG_API_TYPE_DELETE_MESSAGE,
		.chat_id = chat_id,
		.msg_id = message_id,
	};

	int res = tg_api_init(&api);
	if (res != TG_API_ERR_NONE) {
		LOG_ERRN("common", "tg_api_init: %s", tg_api_err_str(res));
		goto out0;
	}

	res = tg_api_exec(&api);
	if (res != TG_API_ERR_NONE) {
		LOG_ERRN("common", "tg_api_exec: %s", tg_api_err_str(res));
		goto out0;
	}

	ret = 0;

out0:
	if (ret < 0)
		_send_error(api.chat_id, api.msg_id, api.user_id, &api, res);

	tg_api_deinit(&api);
	return ret;
}


int
send_text_fmt(const TgMessage *msg, int parse_type, int64_t *ret_id, const char fmt[], ...)
{
	int ret;
	va_list va;

	va_start(va, fmt);
	ret = vsnprintf(NULL, 0, fmt, va);
	va_end(va);

	if (ret < 0)
		return -1;

	const size_t str_len = ((size_t)ret) + 1;
	char *const str = malloc(str_len);
	if (str == NULL)
		return -1;

	va_start(va, fmt);
	ret = vsnprintf(str, str_len, fmt, va);
	va_end(va);

	if (ret < 0)
		goto out0;

	str[ret] = '\0';
	ret = send_text(msg, parse_type, str, ret_id);

out0:
	free(str);
	return ret;
}


int
send_photo_fmt(const TgMessage *msg, int parse_type, int64_t *ret_id, const char photo[],
	       const char fmt[], ...)
{
	int ret;
	va_list va;

	va_start(va, fmt);
	ret = vsnprintf(NULL, 0, fmt, va);
	va_end(va);

	if (ret < 0)
		return -1;

	const size_t str_len = ((size_t)ret) + 1;
	char *const str = malloc(str_len);
	if (str == NULL)
		return -1;

	va_start(va, fmt);
	ret = vsnprintf(str, str_len, fmt, va);
	va_end(va);

	if (ret < 0)
		goto out0;

	str[ret] = '\0';
	ret = send_photo(msg, parse_type, photo, str, ret_id);

out0:
	free(str);
	return ret;
}


/*
 * misc
 */
int
is_admin(int64_t user_id, int64_t chat_id, int64_t owner_id)
{
	if ((owner_id != 0) && (user_id == owner_id))
		return 1;

	const int privs = model_admin_get_privileges(chat_id, user_id);
	if (privs < 0) {
		LOG_ERRN("common", "%s", "is_admin: Failed to get admin list");
		return -1;
	}

	return (privs != 0);
}


char *
tg_escape(const char src[])
{
	return cstr_escape("_*[]()~`>#+-|{}.!", '\\', src);
}


/*
 * Pager
 */
void
pager_list_set(PagerList *p, unsigned curr_page, unsigned per_page, unsigned items_len, unsigned items_size)
{
	assert(curr_page > 0);
	assert(per_page > 0);
	assert(items_len <= items_size);

	p->page_num = curr_page;
	p->page_size = CEIL(items_size, per_page);
	p->items_len = items_len;
	p->items_size = items_size;
}


int
pager_init(Pager *p, const char args[])
{
	/* not a callback */
	if (p->id_callback == NULL) {
		p->page = 1;
		p->udata = args;
		p->created_by = p->id_user;
		return 0;
	}

	SpaceTokenizer st_num;
	const char *next = space_tokenizer_next(&st_num, args);
	if (next == NULL)
		goto err0;

	SpaceTokenizer st_timer;
	next = space_tokenizer_next(&st_timer, next);
	if (next == NULL)
		goto err0;

	SpaceTokenizer st_from_id;
	next = space_tokenizer_next(&st_from_id, next);
	if (next == NULL)
		goto err0;

	/* optional */
	const char *udata = NULL;
	SpaceTokenizer st_udata;
	if (space_tokenizer_next(&st_udata, next) != NULL)
		udata = st_udata.value;

	uint64_t page_num;
	if (cstr_to_uint64_n(st_num.value, st_num.len, &page_num) < 0)
		goto err0;

	int64_t timer;
	if (cstr_to_int64_n(st_timer.value, st_timer.len, &timer) < 0)
		goto err0;

	int64_t from_id;
	if ((cstr_to_int64_n(st_from_id.value, st_from_id.len, &from_id) < 0) && (from_id == 0))
		goto err0;

	if (timer == 0) {
		int _can_delete = 1;
		if (p->id_user != from_id) {
			const int ret = is_admin(p->id_user, p->id_chat, p->id_owner);
			if (ret < 0) {
				ANSWER_CALLBACK_TEXT(p->id_callback, "Failed to get admin list!", 1);
				return -1;
			}

			_can_delete = ret;
		}

		if (_can_delete) {
			const char *_text = "Deleted";
			if (delete_message(p->id_chat, p->id_message) < 0)
				_text = "Failed: Maybe the message is too old or invalid.";

			ANSWER_CALLBACK_TEXT(p->id_callback, _text, 1);
			return -3;
		}

		ANSWER_CALLBACK_TEXT(p->id_callback, "Permission denied!", 1);
		return -1;
	}

	const time_t diff = time(NULL) - timer;
	if (diff >= CFG_LIST_TIMEOUT_S) {
		ANSWER_CALLBACK_TEXT(p->id_callback, "Expired!", 1);
		return -2;
	}

	p->page = (unsigned)page_num;
	p->udata = udata;
	p->created_by = from_id;
	return 0;

err0:
	ANSWER_CALLBACK_TEXT(p->id_callback, "Invalid callback!", 1);
	return -1;
}


int
pager_send(const Pager *p, const PagerList *list, int64_t *ret_id)
{
	if (cstr_is_empty(p->ctx))
		return -1;

	const time_t now = time(NULL);
	const int64_t created_by = p->created_by;
	const unsigned page = list->page_num;
	const TgApiKbdButton btns[] = {
		{
			.text = "Prev",
			.data_list_len = 5,
			.data_list = (TgApiKbdButtonData[]) {
				{ .type = TG_API_KBD_BUTTON_DATA_TYPE_TEXT, .text = p->ctx },
				{ .type = TG_API_KBD_BUTTON_DATA_TYPE_INT64, .int64 = page - 1 },
				{ .type = TG_API_KBD_BUTTON_DATA_TYPE_INT64, .int64 = now },
				{ .type = TG_API_KBD_BUTTON_DATA_TYPE_INT64, .int64 = created_by },
				{ .type = TG_API_KBD_BUTTON_DATA_TYPE_TEXT, .text = p->udata },
			},
		},
		{
			.text = "Next",
			.data_list_len = 5,
			.data_list = (TgApiKbdButtonData[]) {
				{ .type = TG_API_KBD_BUTTON_DATA_TYPE_TEXT, .text = p->ctx },
				{ .type = TG_API_KBD_BUTTON_DATA_TYPE_INT64, .int64 = page + 1 },
				{ .type = TG_API_KBD_BUTTON_DATA_TYPE_INT64, .int64 = now },
				{ .type = TG_API_KBD_BUTTON_DATA_TYPE_INT64, .int64 = created_by },
				{ .type = TG_API_KBD_BUTTON_DATA_TYPE_TEXT, .text = p->udata },
			},
		},
		{
			.text = "Delete",
			.data_list_len = 5,
			.data_list = (TgApiKbdButtonData[]) {
				{ .type = TG_API_KBD_BUTTON_DATA_TYPE_TEXT, .text = p->ctx },
				{ .type = TG_API_KBD_BUTTON_DATA_TYPE_INT64, .int64 = page },
				{ .type = TG_API_KBD_BUTTON_DATA_TYPE_INT64, .int64 = 0 },
				{ .type = TG_API_KBD_BUTTON_DATA_TYPE_INT64, .int64 = created_by },
				{ .type = TG_API_KBD_BUTTON_DATA_TYPE_TEXT, .text = p->udata },
			},
		},
	};

	char *const template = _pager_add_template(p, list);
	if (template == NULL)
		return -1;

	TgApiKbdButton btns_r[LEN(btns)];
	unsigned count = 0;
	if (page < list->page_size)
		btns_r[count++] = btns[1];
	if (page > 1)
		btns_r[count++] = btns[0];

	btns_r[count++] = btns[2];

	const TgApiKbd kbd = {
		.rows_len = 1,
		.rows = &(TgApiKbdRow) {
			.cols_len = count,
			.cols = btns_r,
		},
	};

	int ret = -1;
	TgApi api = {
		.type = (p->id_callback == NULL)? TG_API_TYPE_SEND_TEXT : TG_API_TYPE_EDIT_TEXT,
		.parse_type = TG_API_PARSE_TYPE_FORMAT,
		.value_type = TG_API_VALUE_TYPE_TEXT,
		.chat_id = p->id_chat,
		.msg_id = p->id_message,
		.user_id = p->id_user,
		.value = template,
	};

	int res = tg_api_init(&api);
	if (res != TG_API_ERR_NONE) {
		LOG_ERRN("common", "tg_api_init: %s", tg_api_err_str(res));
		goto out0;
	}

	res = tg_api_add_kbd(&api, &kbd);
	if (res != TG_API_ERR_NONE) {
		LOG_ERRN("common", "tg_api_add_kbd: %s", tg_api_err_str(res));
		goto out1;
	}

	res = tg_api_exec(&api);
	if (res != TG_API_ERR_NONE) {
		LOG_ERRN("common", "tg_api_exec: %s", tg_api_err_str(res));
		goto out1;
	}

	if (ret_id != NULL)
		*ret_id = api.ret_msg_id;

	ret = 0;

out1:
	tg_api_deinit(&api);
out0:
	free(template);
	return ret;
}


/*
 * Private
 */
static int
_exec_with_deleter(TgApi *t, int64_t *ret_id)
{
	int ret = -1;
	int res = tg_api_init(t);
	if (res != TG_API_ERR_NONE) {
		LOG_ERRN("common", "tg_api_init: %s", tg_api_err_str(res));
		goto out0;
	}

	res = _add_deleter(t);
	if (res != TG_API_ERR_NONE) {
		LOG_ERRN("common", "_add_deleter: %s", tg_api_err_str(res));
		goto out0;
	}

	res = tg_api_exec(t);
	if (res != TG_API_ERR_NONE) {
		LOG_ERRN("common", "tg_api_exec: %s", tg_api_err_str(res));
		goto out0;
	}

	if (ret_id != NULL)
		*ret_id = t->ret_msg_id;

	ret = 0;

out0:
	if (ret < 0)
		_send_error(t->chat_id, t->msg_id, t->user_id, t, res);

	tg_api_deinit(t);
	return ret;
}


static void
_send_error(int64_t chat_id, int64_t msg_id, int64_t user_id, const TgApi *api, int err)
{
	if (err == TG_API_ERR_NONE)
		return;

	TgApi new_api = {
		.type = TG_API_TYPE_SEND_TEXT,
		.parse_type = TG_API_PARSE_TYPE_PLAIN,
		.chat_id = chat_id,
		.msg_id = msg_id,
		.user_id = user_id,
	};

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return;

	if (err == TG_API_ERR_API)
		new_api.value = str_set_fmt(&str, "Error: [%d]: %s", api->api_err_code, api->api_err_msg);
	else
		new_api.value = str_set_fmt(&str, "Error: %s", tg_api_err_str(err));

	if (new_api.value != NULL)
		_exec_with_deleter(&new_api, NULL);

	str_deinit(&str);
}


static int
_add_deleter(TgApi *t)
{
	const TgApiKbd kbd = {
		.rows_len = 1,
		.rows = &(TgApiKbdRow) {
			.cols_len = 1,
			.cols = &(TgApiKbdButton) {
				.text = "Delete",
				.data_list_len = 2,
				.data_list = (TgApiKbdButtonData[]) {
					{
						.type = TG_API_KBD_BUTTON_DATA_TYPE_TEXT,
						.text = "/deleter",
					},
					{
						.type = TG_API_KBD_BUTTON_DATA_TYPE_INT64,
						.int64 = t->user_id,
					},
				},
			},
		},
	};

	const int res = tg_api_add_kbd(t, &kbd);
	if (res != TG_API_ERR_NONE)
		LOG_ERRN("common", "tg_api_add_kbd: %s", tg_api_err_str(res));

	return res;
}


static char *
_pager_add_template(const Pager *p, const PagerList *list)
{
	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return NULL;

	return str_append_fmt(&str, "*%s*\n%s\n\\-\\-\\-\nPage\\: \\[%u\\]\\:\\[%u\\] \\- Total\\: %u",
			      cstr_empty_if_null(p->title), cstr_empty_if_null(p->body),
			      list->page_num, list->page_size, list->items_size);
}