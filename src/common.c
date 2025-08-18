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
static char *_fmt(const char fmt[], va_list args);
static int   _pager_delete(const Pager *p, int64_t user_id);
static char *_pager_add_body(const Pager *p, const PagerList *list);
static int   _pager_send(const Pager *p, const TgApiMarkupKbd *kbd, const char body[], int64_t *ret_id);


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

	char *const markup = (msg->from != NULL)? new_deleter(msg->from->id) : NULL;
	const TgApiText api = {
		.type = TG_API_TEXT_TYPE_PLAIN,
		.chat_id = msg->chat.id,
		.msg_id = msg->id,
		.text = text,
		.markup = markup,
	};

	const int ret = _send_text(&api, ret_id);
	free(markup);
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

	char *const markup = (msg->from != NULL)? new_deleter(msg->from->id) : NULL;
	const TgApiText api = {
		.type = TG_API_TEXT_TYPE_FORMAT,
		.chat_id = msg->chat.id,
		.msg_id = msg->id,
		.text = text,
		.markup = markup,
	};

	const int ret = _send_text(&api, ret_id);
	free(markup);
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

	char *const markup = (msg->from != NULL)? new_deleter(msg->from->id) : NULL;
	const TgApiPhoto api = {
		.text_type = TG_API_TEXT_TYPE_PLAIN,
		.chat_id = msg->chat.id,
		.msg_id = msg->id,
		.photo = photo,
		.text = text,
		.markup = markup,
	};

	const int ret = _send_photo(&api, ret_id);
	free(markup);
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

	char *const markup = (msg->from != NULL)? new_deleter(msg->from->id) : NULL;
	const TgApiPhoto api = {
		.text_type = TG_API_TEXT_TYPE_FORMAT,
		.chat_id = msg->chat.id,
		.msg_id = msg->id,
		.photo = photo,
		.text = text,
		.markup = markup,
	};

	const int ret = _send_photo(&api, ret_id);
	free(markup);
	free(text);
	return ret;
}


int
send_error_text(const TgMessage *msg, int64_t *ret_id, const char ctx[], const char fmt[], ...)
{
	char *text;
	va_list va;

	va_start(va, fmt);
	text = _fmt(fmt, va);
	va_end(va);

	int ret = -1;
	if (text == NULL)
		return -1;

	char *const new_text = CSTR_CONCAT("```error\n", ctx, ": ", text, "```");
	if (new_text == NULL)
		goto out0;

	char *const markup = (msg->from != NULL)? new_deleter(msg->from->id) : NULL;
	const TgApiText api = {
		.type = TG_API_TEXT_TYPE_FORMAT,
		.chat_id = msg->chat.id,
		.msg_id = msg->id,
		.text = new_text,
		.markup = markup,
	};

	ret = _send_text(&api, ret_id);
	free(markup);
	free(new_text);

out0:
	free(text);
	return ret;
}


int
send_error_text_nope(const TgMessage *msg, int64_t *ret_id, const char ctx[], const char fmt[], ...)
{
	char *text;
	va_list va;

	va_start(va, fmt);
	text = _fmt(fmt, va);
	va_end(va);

	int ret = -1;
	if (text == NULL)
		return -1;

	char *const new_text = CSTR_CONCAT("Error:\n`", ctx, ": ", text, "`");
	if (new_text == NULL)
		goto out0;

	char *const markup = (msg->from != NULL)? new_deleter(msg->from->id) : NULL;
	const TgApiAnimation api = {
		.text_type = TG_API_TEXT_TYPE_FORMAT,
		.chat_id = msg->chat.id,
		.msg_id = msg->id,
		.animation = "https://github.com/rlapz/assets/raw/refs/heads/main/nope.mp4",
		.text = new_text,
		.markup = markup,
	};

	TgApiResp resp = { 0 };
	ret = tg_api_animation_send(&api, &resp);
	if (ret < 0)
		LOG_ERRN("common", "tg_api_animation_send: %s", resp.error_msg);

	if (ret_id != NULL)
		*ret_id = resp.msg_id;

	free(markup);
	free(new_text);

out0:
	free(text);
	return ret;
}


int
answer_callback_text(const char id[], const char value[], int show_alert)
{
	TgApiResp resp;
	const TgApiCallback api = {
		.value_type = TG_API_CALLBACK_VALUE_TYPE_TEXT,
		.show_alert = show_alert,
		.id = id,
		.value = value,
	};

	const int ret = tg_api_callback_answer(&api, &resp);
	if (ret < 0)
		LOG_ERRN("common", "tg_api_callback_answer: %s", resp.error_msg);

	return ret;
}


int
delete_message(const TgMessage *msg)
{
	TgApiResp resp = { 0 };
	const int ret = tg_api_delete(msg->chat.id, msg->id, &resp);
	if (ret < 0)
		LOG_ERRN("common", "tg_api_delete: %s", resp.error_msg);

	return ret;
}


char *
new_deleter(int64_t user_id)
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

	if (timer == 0)
		return _pager_delete(p, from_id);

	const time_t diff = time(NULL) - timer;
	if (diff >= CFG_LIST_TIMEOUT_S) {
		answer_callback_text(p->id_callback, "Expired!", 1);
		return -2;
	}

	p->page = (unsigned)page_num;
	p->udata = udata;
	p->created_by = from_id;
	return 0;

err0:
	answer_callback_text(p->id_callback, "Invalid callback!", 1);
	return -1;
}


int
pager_send(const Pager *p, const PagerList *list, int64_t *ret_id)
{
	assert(VAL_IS_NULL_OR(p, p->ctx, list) == 0);

	const time_t now = time(NULL);
	const int64_t created_by = p->created_by;
	const unsigned page = list->page_num;
	char *const body = _pager_add_body(p, list);
	if (body == NULL) {
		const TgMessage msg = {
			.id = p->id_message,
			.chat = (TgChat) { .id = p->id_chat },
			.from = &(TgUser) { .id = p->id_user },
		};
		SEND_ERROR_TEXT(&msg, NULL, "%s", "_pager_add_body: failed");
		return -1;
	}

	const TgApiKbdButton btns[] = {
		{
			.label = "Prev",
			.args_len = 5,
			.args = (TgApiKbdButtonArg[]) {
				{ .type = TG_API_KBD_BUTTON_ARG_TYPE_TEXT, .text = p->ctx },
				{ .type = TG_API_KBD_BUTTON_ARG_TYPE_INT64, .int64 = page - 1 },
				{ .type = TG_API_KBD_BUTTON_ARG_TYPE_INT64, .int64 = now },
				{ .type = TG_API_KBD_BUTTON_ARG_TYPE_INT64, .int64 = created_by },
				{ .type = TG_API_KBD_BUTTON_ARG_TYPE_TEXT, .text = p->udata },
			},
		},
		{
			.label = "Next",
			.args_len = 5,
			.args = (TgApiKbdButtonArg[]) {
				{ .type = TG_API_KBD_BUTTON_ARG_TYPE_TEXT, .text = p->ctx },
				{ .type = TG_API_KBD_BUTTON_ARG_TYPE_INT64, .int64 = page + 1 },
				{ .type = TG_API_KBD_BUTTON_ARG_TYPE_INT64, .int64 = now },
				{ .type = TG_API_KBD_BUTTON_ARG_TYPE_INT64, .int64 = created_by },
				{ .type = TG_API_KBD_BUTTON_ARG_TYPE_TEXT, .text = p->udata },
			},
		},
		{
			.label = "Delete",
			.args_len = 5,
			.args = (TgApiKbdButtonArg[]) {
				{ .type = TG_API_KBD_BUTTON_ARG_TYPE_TEXT, .text = p->ctx },
				{ .type = TG_API_KBD_BUTTON_ARG_TYPE_INT64, .int64 = page },
				{ .type = TG_API_KBD_BUTTON_ARG_TYPE_INT64, .int64 = 0 },
				{ .type = TG_API_KBD_BUTTON_ARG_TYPE_INT64, .int64 = created_by },
				{ .type = TG_API_KBD_BUTTON_ARG_TYPE_TEXT, .text = p->udata },
			},
		},
	};

	TgApiKbdButton btns_map[LEN(btns)];
	unsigned count = 0;
	if (page < list->page_size)
		btns_map[count++] = btns[1];
	if (page > 1)
		btns_map[count++] = btns[0];

	btns_map[count++] = btns[2];
	const TgApiMarkupKbd kbd = {
		.rows_len = 1,
		.rows = &(TgApiKbd) {
			.cols_len = count,
			.cols = btns_map,
		},
	};

	const int ret = _pager_send(p, &kbd, body, ret_id);
	free(body);
	return ret;
}


/*
 * Private
 */
static int
_send_text(const TgApiText *t, int64_t *ret_id)
{
	TgApiResp resp = { 0 };
	const int ret = tg_api_text_send(t, &resp);
	if (ret < 0)
		LOG_ERRN("common", "tg_api_text_send: %s", resp.error_msg);

	if (ret_id != NULL)
		*ret_id = resp.msg_id;

	return ret;
}


static int
_send_photo(const TgApiPhoto *t, int64_t *ret_id)
{
	TgApiResp resp = { 0 };
	const int ret = tg_api_photo_send(t, &resp);
	if (ret < 0)
		LOG_ERRN("common", "tg_api_photo_send: %s", resp.error_msg);

	if (ret_id != NULL)
		*ret_id = resp.msg_id;

	return ret;
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


static int
_pager_delete(const Pager *p, int64_t user_id)
{
	int ret = 1;
	if (p->id_user != user_id) {
		ret = is_admin(p->id_user, p->id_chat, p->id_owner);
		if (ret < 0) {
			answer_callback_text(p->id_callback, "Failed to get admin list", 1);
			return -1;
		}
	}

	if (ret == 0) {
		answer_callback_text(p->id_callback, "Permission denied!", 1);
		return -1;
	}

	TgApiResp resp = { 0 };
	ret = tg_api_delete(p->id_chat, p->id_message, &resp);
	if (ret < 0) {
		LOG_ERRN("common", "tg_api_delete: %s", resp.error_msg);
		answer_callback_text(p->id_callback, resp.error_msg, 1);
	} else {
		answer_callback_text(p->id_callback, "Deleted", 0);
	}

	return -3;
}


static char *
_pager_add_body(const Pager *p, const PagerList *list)
{
	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return NULL;

	return str_append_fmt(&str, "*%s*\n%s\n\\-\\-\\-\nPage\\: \\[%u\\]\\:\\[%u\\] \\- Total\\: %u",
			      cstr_empty_if_null(p->title), cstr_empty_if_null(p->body),
			      list->page_num, list->page_size, list->items_size);
}


static int
_pager_send(const Pager *p, const TgApiMarkupKbd *kbd, const char body[], int64_t *ret_id)
{
	char *const markup = tg_api_markup_kbd(kbd);
	if (markup == NULL) {
		const TgMessage msg = {
			.id = p->id_message,
			.chat = (TgChat) { .id = p->id_chat },
			.from = &(TgUser) { .id = p->id_user },
		};
		SEND_ERROR_TEXT(&msg, NULL, "%s", "tg_api_markup_kbd: failed");
		return -1;
	}

	TgApiResp resp = { 0 };
	const TgApiText api = {
		.type = TG_API_TEXT_TYPE_FORMAT,
		.chat_id = p->id_chat,
		.msg_id = p->id_message,
		.text = body,
		.markup = markup,
	};

	int ret;
	if (p->id_callback == NULL)
		ret = tg_api_text_send(&api, &resp);
	else
		ret = tg_api_text_edit(&api, &resp);

	if (ret < 0)
		LOG_ERRN("common", "tg_api_text: %s", resp.error_msg);

	if (ret_id != NULL)
		*ret_id = resp.msg_id;

	free(markup);
	return ret;
}
