#ifndef __TG_API_H__
#define __TG_API_H__


#include <json.h>
#include <stdint.h>

#include <curl/curl.h>

#include "tg.h"
#include "util.h"


typedef enum tg_api_text_type {
	TG_API_TEXT_TYPE_PLAIN,
	TG_API_TEXT_TYPE_FORMAT,
} TgApiTextType;

typedef enum tg_api_photo_type {
	TG_API_PHOTO_TYPE_ID,
	TG_API_PHOTO_TYPE_LINK,
	TG_API_PHOTO_TYPE_FILE,
} TgApiPhotoType;

typedef struct tg_api {
	const char *api;
	CURL       *curl;
	Str         str;
} TgApi;

int  tg_api_init(TgApi *t, const char base_api[]);
void tg_api_deinit(TgApi *t);
int  tg_api_get_me(TgApi *t, TgUser *me, json_object **res);
int  tg_api_send_text(TgApi *t, TgApiTextType type, int64_t chat_id, const int64_t *reply_to,
		      const char text[]);
int  tg_api_send_photo(TgApi *t, TgApiPhotoType type, int64_t chat_id, const int64_t *reply_to,
		       const char caption[], const char src[]);
int  tg_api_get_admin_list(TgApi *t, int64_t chat_id, TgChatAdminList *list, json_object **res,
			   int need_parse);


#endif
