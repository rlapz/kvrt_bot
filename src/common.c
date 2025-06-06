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
 * misc
 */
int
privileges_check(const TgMessage *msg, int64_t owner_id)
{
	if (msg->from->id == owner_id)
		return 1;

	const int privs = model_admin_get_privilegs(msg->chat.id, msg->from->id);
	if (privs < 0) {
		send_text_plain(msg, "Failed to get admin list");
		return -1;
	}

	if (privs == 0) {
		send_text_plain(msg, "Permission denied!");
		return 0;
	}

	return 1;
}


int
is_admin(int64_t user_id, int64_t chat_id, const int64_t *owner_id)
{
	if ((owner_id != NULL) && (user_id == *owner_id))
		return 1;

	const int privs = model_admin_get_privilegs(chat_id, user_id);
	if (privs < 0) {
		log_err(0, "common: is_admin: Falied to get admin list");
		return 0;
	}

	return (privs != 0);
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

	return str_append_fmt(&str, "*%s*\n%s\n\\-\\-\\-\nPage\\: \\[%u\\]\\:\\[%u\\] \\- Total\\: %u",
			      cstr_empty_if_null(l->title), cstr_empty_if_null(l->body),
			      pag->page_count, pag->page_size, pag->items_size);
}


int
message_list_init(MessageList *l, const char args[])
{
	assert(l->msg != NULL);

	/* not a callback */
	if (l->id_callback == NULL) {
		l->page = 1;
		l->udata = args;
		l->created_by = l->id_user;
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
		int should_delete;
		if (l->id_user == from_id)
			should_delete = 1;
		else
			should_delete = is_admin(l->id_user, l->msg->chat.id, &l->id_owner);

		if (should_delete) {
			tg_api_answer_callback_query(l->id_callback, "Deleted", NULL, 0);
			tg_api_delete_message(l->msg->chat.id, l->msg->id);
			return -3;
		}

		tg_api_answer_callback_query(l->id_callback, "Permission denied!", NULL, 1);
		return -1;
	}

	const time_t diff = time(NULL) - timer;
	if (diff >= CFG_LIST_TIMEOUT_S) {
		tg_api_answer_callback_query(l->id_callback, "Expired!", NULL, 1);
		return -2;
	}

	l->page = (unsigned)page_num;
	l->udata = udata;
	l->created_by = from_id;
	return 0;

err0:
	tg_api_answer_callback_query(l->id_callback, "Invalid callback!", NULL, 0);
	return -1;
}


int
message_list_send(const MessageList *l, const MessageListPagination *pag, int64_t *ret_id)
{
	int ret = -1;
	if (cstr_is_empty(l->ctx))
		return ret;

	const time_t now = time(NULL);
	const int64_t created_by = l->created_by;
	const unsigned page = pag->page_count;
	const TgApiInlineKeyboardButton btns[] = {
		{
			.text = "Prev",
			.data = (TgApiInlineKeyboardButtonData[]) {
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->ctx },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = page - 1 },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = now },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = created_by },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->udata },
			},
			.data_len = 5,
		},
		{
			.text = "Next",
			.data = (TgApiInlineKeyboardButtonData[]) {
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->ctx },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = page + 1 },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = now },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = created_by },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->udata },
			},
			.data_len = 5,
		},
		{
			.text = "Delete",
			.data = (TgApiInlineKeyboardButtonData[]) {
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->ctx },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = page },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = 0 },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = created_by },
				{ .type = TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->udata },
			},
			.data_len = 5,
		},
	};


	TgApiInlineKeyboardButton btns_r[LEN(btns)];
	unsigned count = 0;
	if (pag->has_next_page)
		btns_r[count++] = btns[1];

	if (page > 1)
		btns_r[count++] = btns[0];

	btns_r[count++] = btns[2];
	TgApiInlineKeyboard kbds = { .len = count, .buttons = btns_r };

	char *const body_list = _message_list_add_template(l, pag);
	if (body_list == NULL)
		return ret;

	if (l->id_callback == NULL)
		ret = tg_api_send_inline_keyboard(l->msg->chat.id, &l->msg->id, body_list, &kbds, 1, ret_id);
	else
		ret = tg_api_edit_inline_keyboard(l->msg->chat.id, l->msg->id, body_list, &kbds, 1, ret_id);

	free(body_list);
	return ret;
}
