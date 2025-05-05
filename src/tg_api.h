#ifndef __TG_API_H__
#define __TG_API_H__


#include <stdint.h>

#include "tg.h"


enum {
	TG_API_TEXT_TYPE_PLAIN,
	TG_API_TEXT_TYPE_FORMAT,
};

enum {
	TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_INT,
	TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_UINT,
	TG_API_INLINE_KEYBOARD_BUTTON_DATA_TYPE_TEXT,
};

typedef struct tg_api_inline_keyboard_button_data {
	int type;
	union {
		int64_t     int_;
		uint64_t    uint;
		const char *text;
	};
} TgApiInlineKeyboardButtonData;

typedef struct tg_api_inline_keyboard_button {
	const char                          *text;
	const char                          *url;
	const TgApiInlineKeyboardButtonData *data;
	unsigned                             data_len;
} TgApiInlineKeyboardButton;

typedef struct tg_api_inline_keyboard {
	unsigned                         len;
	const TgApiInlineKeyboardButton *buttons;
} TgApiInlineKeyboard;

typedef struct tg_api_answer_callback_query {
	const char *id;
	const char *text;
	const char *url;
	int         show_alert;
	int         cache_time;
} TgApiAnswerCallbackQuery;


void tg_api_init(const char base_api[]);
int  tg_api_send_text(int type, int64_t chat_id, const int64_t *reply_to, const char text[], int64_t *ret_id);
int  tg_api_delete_message(int64_t chat_id, int64_t message_id);
int  tg_api_send_inline_keyboard(int64_t chat_id, const int64_t *reply_to, const char text[],
				 const TgApiInlineKeyboard kbds[], unsigned kbds_len, int64_t *ret_id);
int  tg_api_edit_inline_keyboard(int64_t chat_id, int64_t msg_id, const char text[],
				 const TgApiInlineKeyboard kbds[], unsigned kbds_len, int64_t *ret_id);
int  tg_api_answer_callback_query(const char id[], const char text[], const char url[], int show_alert);
int  tg_api_get_admin_list(int64_t chat_id, TgChatAdminList *list, json_object **res_obj);


#endif
