#include <assert.h>
#include <string.h>
#include <time.h>
#include <math.h>

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
static char *
_message_list_add_template(const MessageList *l, const MessageListPagination *pag)
{
	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return NULL;

	return str_append_fmt(&str, "*%s*\n%s\n\n\\-\\-\\-\nPage\\: \\[%u\\]\\:\\[%u\\] \\- Total\\: %u",
			      l->title, l->body, pag->current_page, pag->total_page, pag->max_len);
}


int
message_list_send(const MessageList *l, const MessageListPagination *pag, int64_t *ret_id)
{
	int ret = -1;
	if (cstr_is_empty(l->ctx))
		return ret;

	const time_t now = time(NULL);
	const unsigned page = pag->current_page;
	const TgApiInlineKeyboardButton btns[] = {
		{
			.text = "Prev",
			.data = (TgApiInlineKeyboardButtonData[]) {
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->ctx },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = page - 1 },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = now },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->udata },
			},
			.data_len = 4,
		},
		{
			.text = "Next",
			.data = (TgApiInlineKeyboardButtonData[]) {
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->ctx },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = page + 1 },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = now },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->udata },
			},
			.data_len = 4,
		},
	};

	TgApiInlineKeyboard kbds = { 0 };
	if (pag->has_next_page) {
		kbds.len++;
		kbds.buttons = &btns[1];
	}

	if (page > 1) {
		kbds.len++;
		kbds.buttons = btns;
	}

	char *const body_list = _message_list_add_template(l, pag);
	if (body_list == NULL)
		return ret;

	if (l->is_edit)
		ret = tg_api_edit_inline_keyboard(l->msg->chat.id, l->msg->id, body_list, &kbds, 1, ret_id);
	else
		ret = tg_api_send_inline_keyboard(l->msg->chat.id, &l->msg->id, body_list, &kbds, 1, ret_id);

	free(body_list);
	return ret;
}


int
message_list_get_page(const char id[], const char args[], unsigned *page)
{
	SpaceTokenizer st_num;
	const char *const next = space_tokenizer_next(&st_num, args);
	if (next == NULL)
		return -1;

	SpaceTokenizer st_timer;
	if (space_tokenizer_next(&st_timer, next) == NULL)
		return -1;

	uint64_t page_num;
	if (cstr_to_uint64_n(st_num.value, st_num.len, &page_num) < 0)
		return -1;

	uint64_t timer;
	if (cstr_to_uint64_n(st_timer.value, st_timer.len, &timer) < 0)
		return -1;

	const time_t diff = time(NULL) - timer;
	if (diff >= CFG_LIST_TIMEOUT_S) {
		tg_api_answer_callback_query(id, "Expired!", NULL, 1);
		return -1;
	}

	*page = (unsigned)page_num;
	return 0;
}

