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


/* new api */

enum {
	TG_API_TYPE_SEND_TEXT,
	TG_API_TYPE_SEND_PHOTO,
	TG_API_TYPE_EDIT_TEXT,
	TG_API_TYPE_EDIT_CAPTION,
	TG_API_TYPE_DELETE_MESSAGE,
	TG_API_TYPE_ANSWER_CALLBACK,
};


//enum {
//	TG_API_TEXT_TYPE_PLAIN,
//	TG_API_TEXT_TYPE_FORMAT,
//};

enum {
	TG_API_ERR_NONE  = 0,
	TG_API_ERR_API   = 1,

	TG_API_ERR_TYPE_INVALID         = -1,
	TG_API_ERR_TYPE_TEXT_INVALID    = -2,
	TG_API_ERR_VALUE_INVALID        = -3,
	TG_API_ERR_CAPTION_INVALID      = -4,
	TG_API_ERR_CHAT_ID_INVALID      = -5,
	TG_API_ERR_USER_ID_INVALID      = -6,
	TG_API_ERR_MSG_ID_INVALID       = -7,
	TG_API_ERR_REPLY_MSG_ID_INVALID = -8,
	TG_API_ERR_HAS_NO_TYPE          = -9,

	TG_API_ERR_INVALID_RESP = -1021,
	TG_API_ERR_SEND_REQ     = -1022,
	TG_API_ERR_INTERNAL     = -1023,
	TG_API_ERR_UNKNOWN      = -1024,
};

void        tg_api_global_init(const char base_api[]);
const char *tg_api_err_str(int err);


typedef struct tg_api {
	int         type;
	int         text_type;
	const char *caption;
	const char *value;
	int64_t     chat_id;
	int64_t     msg_id;
	int64_t     user_id;

	/* filled by the callee */
	int64_t      ret_msg_id;
	int          api_err_code;
	const char  *api_err_msg;
	json_object *res_json;

	/* internal */
	Str str;
	int has_msg_id;
} TgApi;

int  tg_api_init1(TgApi *t);
void tg_api_deinit(TgApi *t);
int  tg_api_exec(TgApi *t);


/*
 * Keyboard
 */
enum {
	TG_API_KBD_BUTTON_DATA_TYPE_INT64,
	TG_API_KBD_BUTTON_DATA_TYPE_UINT64,
	TG_API_KBD_BUTTON_DATA_TYPE_TEXT,
};


typedef struct tg_api_kbd_button_data {
	int type;
	union {
		int64_t     int64;
		uint64_t    uint64;
		const char *text;
	};
} TgApiKbdButtonData;

typedef struct tg_api_kbd_button {
	const char               *text;
	const char               *url;
	const TgApiKbdButtonData *data_list;
	unsigned                  data_list_len;
} TgApiKbdButton;

typedef struct tg_api_kbd_row {
	unsigned              cols_len;
	const TgApiKbdButton *cols;
} TgApiKbdRow;

typedef struct tg_api_kbd {
	unsigned           rows_len;
	const TgApiKbdRow *rows;
} TgApiKbd;

int  tg_api_add_kbd(TgApi *t, const TgApiKbd *kbd);


#endif
