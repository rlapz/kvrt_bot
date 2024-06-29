#ifndef __TG_API_H__
#define __TG_API_H__


#include <stdint.h>

#include <curl/curl.h>

#include "util.h"


typedef struct tg_api {
	const char *api;
	size_t      api_offt;
	CURL       *curl;
	Str         str_compose;
	Str         str_response;
} TgApi;

int  tg_api_init(TgApi *t, const char base_api[]);
void tg_api_deinit(TgApi *t);
int  tg_api_send_text_plain(TgApi *t, int64_t chat_id, int64_t reply_to, const char text[]);
int  tg_api_send_text_format(TgApi *t, int64_t chat_id, int64_t reply_to, const char text[]);


#endif
