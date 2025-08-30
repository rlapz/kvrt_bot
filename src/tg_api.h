#ifndef __TG_API_H__
#define __TG_API_H__


#include <stdint.h>

#include "tg.h"


void tg_api_init(const char base_url[]);


/*
 * Error codes:
 *    < 0           : System error
 *      0           : Success
 * >= 100 and <= 500: Http error codes
 */

enum {
	TG_API_RESP_ERR_TYPE_NONE,
	TG_API_RESP_ERR_TYPE_ARG,
	TG_API_RESP_ERR_TYPE_API,
	TG_API_RESP_ERR_TYPE_SYS,
};

typedef struct tg_api_resp {
	int64_t msg_id;

	int  err_type;
	int  error_code;
	char error_msg[256];
} TgApiResp;


/*
 * Text
 */
enum {
	TG_API_TEXT_TYPE_PLAIN,
	TG_API_TEXT_TYPE_FORMAT,
};

typedef struct tg_api_text {
	int         type;
	int64_t     chat_id;
	int64_t     msg_id;
	const char *text;
	const char *markup;
} TgApiText;

int tg_api_text_send(const TgApiText *t, TgApiResp *resp);
int tg_api_text_edit(const TgApiText *t, TgApiResp *resp);


/*
 * Photo
 */
typedef struct tg_api_photo {
	int         text_type;
	int64_t     chat_id;
	int64_t     msg_id;
	const char *photo;
	const char *text;
	const char *markup;
} TgApiPhoto;

int tg_api_photo_send(const TgApiPhoto *t, TgApiResp *resp);


/*
 * Animation
 */
typedef struct tg_api_animation {
	int         text_type;
	int64_t     chat_id;
	int64_t     msg_id;
	const char *animation;
	const char *text;
	const char *markup;
} TgApiAnimation;

int tg_api_animation_send(const TgApiAnimation *t, TgApiResp *resp);


/*
 * Caption
 */
typedef struct tg_api_caption {
	int         text_type;
	int64_t     chat_id;
	int64_t     msg_id;
	const char *text;
	const char *markup;
} TgApiCaption;

int tg_api_caption_edit(const TgApiCaption *t, TgApiResp *resp);


/*
 * Callback
 */
enum {
	TG_API_CALLBACK_VALUE_TYPE_TEXT,
	TG_API_CALLBACK_VALUE_TYPE_URL,
};

typedef struct tg_api_callback {
	int         value_type;
	int         show_alert;
	const char *id;
	const char *value;
} TgApiCallback;

int tg_api_callback_answer(const TgApiCallback *t, TgApiResp *resp);


/*
 * Delete
 */
int tg_api_delete(int64_t chat_id, int64_t msg_id, TgApiResp *resp);


/*
 * Markup
 */
enum {
	TG_API_KBD_BUTTON_ARG_TYPE_INT64,
	TG_API_KBD_BUTTON_ARG_TYPE_UINT64,
	TG_API_KBD_BUTTON_ARG_TYPE_TEXT,
};

typedef struct tg_api_kbd_button_arg {
	int type;
	union {
		int64_t     int64;
		uint64_t    uint64;
		const char *text;
	};
} TgApiKbdButtonArg;

typedef struct tg_api_kbd_button {
	const char              *label;
	const char              *url;
	unsigned                 args_len;
	const TgApiKbdButtonArg *args;
} TgApiKbdButton;

typedef struct tg_api_kbd {
	unsigned              cols_len;
	const TgApiKbdButton *cols;
} TgApiKbd;

typedef struct tg_api_markup_kbd {
	unsigned        rows_len;
	const TgApiKbd *rows;
} TgApiMarkupKbd;

char *tg_api_markup_kbd(const TgApiMarkupKbd *t);


/*
 * Misc
 */
int tg_api_get_admin_list(TgChatAdminList *list, int64_t chat_id, TgApiResp *resp);


#endif
