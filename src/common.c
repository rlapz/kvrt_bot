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


static char *_message_list_add_template(const MessageList *l, const MessageListPagination *pag);
static int   _send_text_add_deleter(const TgMessage *msg, const char text[], int64_t *ret_id);
static int   _send_photo_add_deleter(const TgMessage *msg, const char photo[], const char caption[],
				     int64_t *ret_id);


/*
 * tg_api wrappers
 */
int
send_text_fmt(const TgMessage *msg, int type, int deletable, int64_t *ret_id, const char fmt[], ...)
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
	if (deletable == 0) {
		ret = tg_api_send_text(type, msg->chat.id, msg->id, str, ret_id);
		goto out0;
	}

	ret = -1;
	char *new_str;
	switch (type) {
	case TG_API_TEXT_TYPE_PLAIN:
		new_str = tg_escape(str);
		if (str != NULL)
			ret = _send_text_add_deleter(msg, new_str, ret_id);

		free(new_str);
		break;
	case TG_API_TEXT_TYPE_FORMAT:
		ret = _send_text_add_deleter(msg, str, ret_id);
		break;
	}

out0:
	free(str);
	return ret;
}


int
send_photo_fmt(const TgMessage *msg, int type, int deletable, int64_t *ret_id, const char photo[],
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
	if (deletable == 0)
		ret = tg_api_send_photo(type, msg->chat.id, msg->id, photo, str, ret_id);
	else
		ret = _send_photo_add_deleter(msg, photo, str, ret_id);

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
 * MessageList
 */
void
message_list_pagination_set(MessageListPagination *m, unsigned curr_page, unsigned per_page,
			    unsigned items_len, unsigned items_size)
{
	assert(curr_page > 0);
	assert(per_page > 0);
	assert(items_len <= items_size);

	m->page_num = curr_page;
	m->page_size = CEIL(items_size, per_page);
	m->items_len = items_len;
	m->items_size = items_size;
}


int
message_list_init(MessageList *l, const char args[])
{
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
		int _can_delete = 1;
		if (l->id_user != from_id) {
			const int ret = is_admin(l->id_user, l->id_chat, l->id_owner);
			if (ret < 0) {
				ANSWER_CALLBACK_TEXT(l->id_callback, "Failed to get admin list!", 1);
				return -1;
			}

			_can_delete = ret;
		}

		if (_can_delete) {
			const char *_text = "Deleted";
			if (tg_api_delete_message(l->id_chat, l->id_message) < 0)
				_text = "Failed: Maybe the message is too old or invalid.";

			ANSWER_CALLBACK_TEXT(l->id_callback, _text, 1);
			return -3;
		}

		ANSWER_CALLBACK_TEXT(l->id_callback, "Permission denied!", 1);
		return -1;
	}

	const time_t diff = time(NULL) - timer;
	if (diff >= CFG_LIST_TIMEOUT_S) {
		ANSWER_CALLBACK_TEXT(l->id_callback, "Expired!", 1);
		return -2;
	}

	l->page = (unsigned)page_num;
	l->udata = udata;
	l->created_by = from_id;
	return 0;

err0:
	ANSWER_CALLBACK_TEXT(l->id_callback, "Invalid callback!", 1);
	return -1;
}


int
message_list_send(const MessageList *l, const MessageListPagination *pag, int64_t *ret_id)
{
	if (cstr_is_empty(l->ctx))
		return -1;

	const time_t now = time(NULL);
	const int64_t created_by = l->created_by;
	const unsigned page = pag->page_num;
	const TgApiKeyboardButton btns[] = {
		{
			.text = "Prev",
			.data = (TgApiKeyboardButtonData[]) {
				{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->ctx },
				{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = page - 1 },
				{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = now },
				{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = created_by },
				{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->udata },
			},
			.data_len = 5,
		},
		{
			.text = "Next",
			.data = (TgApiKeyboardButtonData[]) {
				{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->ctx },
				{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = page + 1 },
				{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = now },
				{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = created_by },
				{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->udata },
			},
			.data_len = 5,
		},
		{
			.text = "Delete",
			.data = (TgApiKeyboardButtonData[]) {
				{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->ctx },
				{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = page },
				{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = 0 },
				{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = created_by },
				{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = l->udata },
			},
			.data_len = 5,
		},
	};

	char *const body_list = _message_list_add_template(l, pag);
	if (body_list == NULL)
		return -1;

	TgApiKeyboardButton btns_r[LEN(btns)];
	unsigned count = 0;
	if (page < pag->page_size)
		btns_r[count++] = btns[1];

	if (page > 1)
		btns_r[count++] = btns[0];

	btns_r[count++] = btns[2];

	const TgApiKeyboard kbd = {
		.type = (l->id_callback == NULL)? TG_API_KEYBOARD_TYPE_TEXT : TG_API_KEYBOARD_TYPE_EDIT_TEXT,
		.value = body_list,
		.rows_len = 1,
		.rows = &(TgApiKeyboardRow) {
			.cols_len = count,
			.cols = btns_r,
		},
	};

	const int ret = tg_api_send_keyboard(&kbd, l->id_chat, l->id_message, ret_id);
	free(body_list);
	return ret;
}


/*
 * Private
 */
static char *
_message_list_add_template(const MessageList *l, const MessageListPagination *pag)
{
	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return NULL;

	return str_append_fmt(&str, "*%s*\n%s\n\\-\\-\\-\nPage\\: \\[%u\\]\\:\\[%u\\] \\- Total\\: %u",
			      cstr_empty_if_null(l->title), cstr_empty_if_null(l->body),
			      pag->page_num, pag->page_size, pag->items_size);
}


static int
_send_text_add_deleter(const TgMessage *msg, const char text[], int64_t *ret_id)
{
	const TgApiKeyboard kbd = {
		.type = TG_API_KEYBOARD_TYPE_TEXT,
		.value = text,
		.rows_len = 1,
		.rows = &(TgApiKeyboardRow) {
			.cols_len = 1,
			.cols = &(TgApiKeyboardButton) {
				.text = "Delete",
				.data_len = 2,
				.data = (TgApiKeyboardButtonData[]) {
					{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = "/deleter" },
					{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = msg->from->id },
				},
			},
		},
	};

	return tg_api_send_keyboard(&kbd, msg->chat.id, msg->id, ret_id);
}


static int
_send_photo_add_deleter(const TgMessage *msg, const char photo[], const char caption[], int64_t *ret_id)
{
	const TgApiKeyboard kbd = {
		.type = TG_API_KEYBOARD_TYPE_PHOTO,
		.value = photo,
		.caption = caption,
		.rows_len = 1,
		.rows = &(TgApiKeyboardRow) {
			.cols_len = 1,
			.cols = &(TgApiKeyboardButton) {
				.text = "Delete",
				.data_len = 2,
				.data = (TgApiKeyboardButtonData[]) {
					{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_TEXT, .text = "/deleter" },
					{ .type = TG_API_KEYBOARD_BUTTON_DATA_TYPE_INT, .int_ = msg->from->id },
				},
			},
		},
	};

	return tg_api_send_keyboard(&kbd, msg->chat.id, msg->id, ret_id);
}
