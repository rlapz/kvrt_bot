#include <string.h>
#include <time.h>

#include "common.h"

#include "config.h"
#include "model.h"
#include "tg_api.h"
#include "util.h"


/*
 * BotCmd
 */
int
bot_cmd_parse(BotCmd *b, const char uname[], const char src[])
{
	SpaceTokenizer st;
	const char *next = space_tokenizer_next(&st, src);

	/* verify username & skip username */
	const char *const _uname = strchr(st.value, '@');
	const char *const _uname_end = st.value + st.len;
	if ((_uname != NULL) && (_uname < _uname_end)) {
		if (cstr_casecmp_n(uname, _uname, (size_t)(_uname_end - _uname)) == 0)
			return -1;

		b->name_len = (unsigned)(_uname - st.value);
		b->has_username = 1;
	} else {
		b->name_len = st.len;
		b->has_username = 0;
	};

	b->name = st.value;
	b->args = next;
	return 0;
}


/*
 * CallbackQuery
 */
int
callback_query_parse(CallbackQuery *c, const char src[])
{
	SpaceTokenizer st;
	const char *next = space_tokenizer_next(&st, src);
	if (st.len == 0)
		return -1;

	c->ctx = st.value;
	c->ctx_len = st.len;
	c->args = next;
	return 0;
}


/*
 * misc
 */
int
privileges_check(const TgMessage *msg, int64_t owner_id)
{
	const char *resp;
	if (msg->from->id == owner_id)
		return 1;

	const int privs = model_admin_get_privilegs(msg->chat.id, msg->from->id);
	if (privs < 0) {
		resp = "Falied to get admin list";
		goto out0;
	}

	if (privs == 0) {
		resp = "Permission denied!";
		goto out0;
	}

	return 1;

out0:
	send_text_plain(msg, resp);
	return 0;
}


char *
tg_escape(const char src[])
{
	return cstr_escape("_*[]()~`>#+-|{}.!", '\\', src);
}


/*
 * MessageList
 */
int
_message_list_add_template(const MessageList *l, Str *str)
{
	const MessageListPagination *const p = &l->pagination;
	if (str_append_fmt(str, "*%s*\n%s\n\n\\-\\-\\-\nPage\\: \\[%u\\]\\:\\[%u\\] \\- "
				"Total\\: %u", l->title, l->body, p->current_page,
					       p->total_page, p->total) == NULL) {
		return -1;
	}

	return 0;
}


int
message_list_paginate(const MessageList *l, int64_t *ret_id)
{
	int ret = -1;
	const MessageListPagination *const p = &l->pagination;

	Str str;
	if (str_init_alloc(&str, 0) < 0)
		return -1;

	if (_message_list_add_template(l, &str) < 0)
		goto out0;

	const time_t now = time(NULL);
	const TgApiInlineKeyboardButton btns[] = {
		{
			.text = "Prev",
			.data = (TgApiInlineKeyboardButtonData[]) {
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->ctx },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = p->current_page - 1 },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = now },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->udata },
			},
			.data_len = 4,
		},
		{
			.text = "Next",
			.data = (TgApiInlineKeyboardButtonData[]) {
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->ctx },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = p->current_page + 1 },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = now },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->udata },
			},
			.data_len = 4,
		},
	};

	TgApiInlineKeyboard kbds = { 0 };
	/* next */
	if (p->has_next_page) {
		kbds.len++;
		kbds.buttons = &btns[1];
	}

	/* prev */
	if (p->current_page > 1) {
		kbds.len++;
		kbds.buttons = btns;
	}

	ret = tg_api_edit_inline_keyboard(l->msg->chat.id, l->msg->id, str.cstr, &kbds,
					  1, ret_id);

out0:
	str_deinit(&str);
	return ret;
}


int
message_list_send(const MessageList *l, int64_t *ret_id)
{
	Str str;
	if (str_init_alloc(&str, 0) < 0)
		return -1;

	int ret = -1;
	if (_message_list_add_template(l, &str) < 0)
		goto out0;

	const unsigned next_page = l->pagination.current_page - 1;
	const TgApiInlineKeyboard kbd = {
		.buttons = &(TgApiInlineKeyboardButton) {
			.text = "Next",
			.data = (TgApiInlineKeyboardButtonData[]) {
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->ctx },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = next_page },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = time(NULL) },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->udata },
			},
			.data_len = 4,
		},
		.len = 1,
	};

	ret = tg_api_send_inline_keyboard(l->msg->chat.id, &l->msg->id, str.cstr, &kbd, 1, ret_id);

out0:
	str_deinit(&str);
	return ret;
}


int
message_list_check_expiration(const char id[], time_t prev)
{
	const time_t diff = time(NULL) - prev;
	if (diff >= CFG_LIST_TIMEOUT_S) {
		if (tg_api_answer_callback_query(id, "Expired!", NULL, 1) < 0)
			return -1;

		return 0;
	}

	return 1;
}
