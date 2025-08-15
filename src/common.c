#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "common.h"

#include "config.h"
#include "model.h"
#include "tg_api.h"
#include "util.h"


static int   _send_text(const TgApiText *t, int64_t *ret_id);
static int   _send_photo(const TgApiPhoto *t, int64_t *ret_id);
static char *_add_deleter(int64_t user_id);
static char *_fmt(const char fmt[], va_list args);


/*
 * tg_api wrappers
 */
int
send_text_plain(const TgMessage *msg, int64_t *ret_id, const char fmt[], ...)
{
	char *text;
	va_list va;

	va_start(va, fmt);
	text = _fmt(fmt, va);
	va_end(va);

	if (text == NULL)
		return -1;

	const TgApiText api = {
		.type = TG_API_TEXT_TYPE_PLAIN,
		.chat_id = msg->chat.id,
		.msg_id = msg->id,
		.text = text,
		.markup = _add_deleter(msg->from->id),
	};

	const int ret = _send_text(&api, ret_id);
	free((char *)api.markup);

out0:
	free(text);
	return ret;
}


int
send_text_format(const TgMessage *msg, int64_t *ret_id, const char fmt[], ...)
{
	char *text;
	va_list va;

	va_start(va, fmt);
	text = _fmt(fmt, va);
	va_end(va);

	if (text == NULL)
		return -1;

	const TgApiText api = {
		.type = TG_API_TEXT_TYPE_FORMAT,
		.chat_id = msg->chat.id,
		.msg_id = msg->id,
		.text = text,
		.markup = _add_deleter(msg->from->id),
	};

	const int ret = _send_text(&api, ret_id);
	free((char *)api.markup);

out0:
	free(text);
	return ret;
}


int
send_photo_plain(const TgMessage *msg, int64_t *ret_id, const char photo[], const char fmt[], ...)
{
	char *text;
	va_list va;

	va_start(va, fmt);
	text = _fmt(fmt, va);
	va_end(va);

	if (text == NULL)
		return -1;

	const TgApiPhoto api = {
		.text_type = TG_API_TEXT_TYPE_PLAIN,
		.chat_id = msg->chat.id,
		.msg_id = msg->id,
		.photo = photo,
		.text = text,
		.markup = _add_deleter(msg->from->id),
	};

	const int ret = _send_photo(&api, ret_id);
	free((char *)api.markup);

out0:
	free(text);
	return ret;
}


int
send_photo_format(const TgMessage *msg, int64_t *ret_id, const char photo[], const char fmt[], ...)
{
	char *text;
	va_list va;

	va_start(va, fmt);
	text = _fmt(fmt, va);
	va_end(va);

	if (text == NULL)
		return -1;

	const TgApiPhoto api = {
		.text_type = TG_API_TEXT_TYPE_FORMAT,
		.chat_id = msg->chat.id,
		.msg_id = msg->id,
		.photo = photo,
		.text = text,
		.markup = _add_deleter(msg->from->id),
	};

	const int ret = _send_photo(&api, ret_id);
	free((char *)api.markup);

out0:
	free(text);
	return ret;
}


int
send_error_text(const TgMessage *msg, int64_t *ret_id, const char fmt[], ...)
{
	char *text;
	va_list va;

	va_start(va, fmt);
	text = _fmt(fmt, va);
	va_end(va);

	int ret = -1;
	if (text == NULL)
		return -1;

	char *const new_text = CSTR_CONCAT("```error\n", text, "```");
	if (new_text == NULL)
		goto out0;

	const TgApiText api = {
		.type = TG_API_TEXT_TYPE_FORMAT,
		.chat_id = msg->chat.id,
		.msg_id = msg->id,
		.text = new_text,
		.markup = _add_deleter(msg->from->id),
	};

	ret = _send_text(&api, ret_id);
	free((char *)api.markup);
	free(new_text);

out0:
	free(text);
	return ret;
}


int
answer_callback_text(const char id[], const char value[], int show_alert)
{
	TgApiResp resp = { 0 };
	char buff[1024];

	const TgApiCallback api = {
		.value_type = TG_API_CALLBACK_VALUE_TYPE_TEXT,
		.show_alert = show_alert,
		.id = id,
		.value = value,
	};

	const int ret = tg_api_callback_answer(&api, &resp);
	if (ret == TG_API_RESP_ERR_API)
		LOG_ERRN("common", "tg_api_callback_answer: %s", tg_api_resp_str(&resp, buff, LEN(buff)));
	else if (ret < 0)
		LOG_ERRN("common", "tg_api_callback_answer: errnum: %d", ret);

	return ret;
}


int
delete_message(const TgMessage *msg)
{
	TgApiResp resp = { 0 };
	char buff[1024];

	const int ret = tg_api_delete(msg->chat.id, msg->id, &resp);
	if (ret == TG_API_RESP_ERR_API)
		LOG_ERRN("common", "tg_api_delete: %s", tg_api_resp_str(&resp, buff, LEN(buff)));
	else if (ret < 0)
		LOG_ERRN("common", "tg_api_delete: errnum: %d", ret);

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
pager_list_set(PagerList *p, unsigned curr_page, unsigned per_page, unsigned items_len,
	       unsigned items_size)
{
}


int
pager_init(Pager *p, const char args[])
{
	return 0;
}


int
pager_send(const Pager *p, const PagerList *list, int64_t *ret_id)
{
	return 0;
}


/*
 * Private
 */
static int
_send_text(const TgApiText *t, int64_t *ret_id)
{
	TgApiResp resp = { 0 };
	char buff[1024];

	const int ret = tg_api_text_send(t, &resp);
	if (ret == TG_API_RESP_ERR_API)
		LOG_ERRN("common", "tg_api_text_send: %s", tg_api_resp_str(&resp, buff, LEN(buff)));
	else if (ret < 0)
		LOG_ERRN("common", "tg_api_text_send: errnum: %d", ret);

	if (ret_id != NULL)
		*ret_id = resp.msg_id;

	return ret;
}


static int
_send_photo(const TgApiPhoto *t, int64_t *ret_id)
{
	TgApiResp resp = { 0 };
	char buff[1024];

	const int ret = tg_api_photo_send(t, &resp);
	if (ret == TG_API_RESP_ERR_API)
		LOG_ERRN("common", "tg_api_photo_send: %s", tg_api_resp_str(&resp, buff, LEN(buff)));
	else if (ret < 0)
		LOG_ERRN("common", "tg_api_photo_send: errnum: %d", ret);

	if (ret_id != NULL)
		*ret_id = resp.msg_id;

	return ret;
}


static char *
_add_deleter(int64_t user_id)
{
	const TgApiKbdButton btn = {
		.label = "Delete",
		.args_len = 2,
		.args = (TgApiKbdButtonArg[]) {
			{
				.type = TG_API_KBD_BUTTON_ARG_TYPE_TEXT,
				.text = "/deleter",
			},
			{
				.type = TG_API_KBD_BUTTON_ARG_TYPE_INT64,
				.int64 = user_id,
			},
		},
	};

	const TgApiMarkupKbd kbd = {
		.rows_len = 1,
		.rows = &(TgApiKbd) {
			.cols_len = 1,
			.cols = &btn,
		},
	};

	return tg_api_markup_kbd(&kbd);
}


static char *
_fmt(const char fmt[], va_list args)
{
	va_list va;
	va_copy(va, args);

	int ret = vsnprintf(NULL, 0, fmt, va);
	if (ret < 0)
		return NULL;

	const size_t len = ((size_t)ret) + 1;
	char *const res = malloc(len);
	if (res == NULL)
		return NULL;

	va_copy(va, args);
	ret = vsnprintf(res, len, fmt, va);
	if (ret < 0) {
		free(res);
		return NULL;
	}

	res[ret] = '\0';
	return res;
}
