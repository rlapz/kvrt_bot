#ifndef __TG_API_H__
#define __TG_API_H__


#include <stdint.h>

#include <curl/curl.h>

#include "util.h"


typedef enum tg_api_text_type {
	TG_API_TEXT_TYPE_PLAIN,
	TG_API_TEXT_TYPE_FORMAT,
} TgApiTextType;

typedef struct tg_api {
	const char *api;
	size_t      api_offt;
	CURL       *curl;
	Str         str_compose;
	Str         str_response;
} TgApi;

int  tg_api_init(TgApi *t, const char base_api[]);
void tg_api_deinit(TgApi *t);
int  tg_api_send_text(TgApi *t, TgApiTextType type, int64_t chat_id, const int64_t *reply_to, const char text[]);


#endif
