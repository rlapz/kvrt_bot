#ifndef __TG_API_H__
#define __TG_API_H__


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
int  tg_api_send_text_plain(TgApi *t, const char chat_id[], const char reply_to[],
			    const char text[]);


#endif
