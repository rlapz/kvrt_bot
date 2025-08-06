#ifndef __TG_API_H__
#define __TG_API_H__


#include <stdint.h>

#include "tg.h"


enum {
	TG_API_TEXT_TYPE_PLAIN,
	TG_API_TEXT_TYPE_FORMAT,
};

enum {
	TG_API_PHOTO_TYPE_URL,
	TG_API_PHOTO_TYPE_FILE,
};

enum {
	TG_API_EDIT_TYPE_TEXT_PLAIN,
	TG_API_EDIT_TYPE_CAPTION_PLAIN,
	TG_API_EDIT_TYPE_TEXT_FORMAT,
	TG_API_EDIT_TYPE_CAPTION_FORMAT,
};

enum {
	TG_API_KEYBOARD_TYPE_TEXT,
	TG_API_KEYBOARD_TYPE_PHOTO,
	TG_API_KEYBOARD_TYPE_EDIT_TEXT,
	TG_API_KEYBOARD_TYPE_EDIT_CAPTION,
};

enum {
	TG_API_KEYBOARD_BUTTON_DATA_TYPE_INT,
	TG_API_KEYBOARD_BUTTON_DATA_TYPE_UINT,
	TG_API_KEYBOARD_BUTTON_DATA_TYPE_TEXT,
};

enum {
	TG_API_ANSWER_CALLBACK_TYPE_TEXT,
	TG_API_ANSWER_CALLBACK_TYPE_URL,
};


typedef struct tg_api_keyboard_button_data {
	int type;
	union {
		int64_t     int_;
		uint64_t    uint;
		const char *text;
	};
} TgApiKeyboardButtonData;

typedef struct tg_api_keyboard_button {
	const char                    *text;
	const char                    *url;
	const TgApiKeyboardButtonData *data;
	unsigned                       data_len;
} TgApiKeyboardButton;

typedef struct tg_api_keyboard_row {
	unsigned                   cols_len;
	const TgApiKeyboardButton *cols;
} TgApiKeyboardRow;

typedef struct tg_api_keyboard {
	int                     type;
	const char             *value;
	const char             *caption;
	unsigned                rows_len;
	const TgApiKeyboardRow *rows;
} TgApiKeyboard;


void tg_api_init(const char base_api[]);
int  tg_api_send_text(int type, int64_t chat_id, int64_t reply_to, const char text[], int64_t *ret_id);
int  tg_api_send_photo(int type, int64_t chat_id, int64_t reply_to, const char photo[], const char capt[],
		       int64_t *ret_id);
int  tg_api_delete_message(int64_t chat_id, int64_t message_id);
int  tg_api_edit_message(int type, int64_t chat_id, int64_t message_id, const char value[]);
int  tg_api_send_keyboard(const TgApiKeyboard *k, int64_t chat_id, int64_t reply_to, int64_t *ret_id);
int  tg_api_answer_callback(int type, const char id[], const char arg[], int show_alert);
int  tg_api_get_admin_list(int64_t chat_id, TgChatAdminList *list, json_object **res_obj);


#endif
