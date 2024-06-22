#ifndef __TG_H__
#define __TG_H__


#include <stddef.h>
#include <stdint.h>
#include <json.h>

#include <curl/curl.h>

#include "util.h"


/* User */
typedef struct {
	int64_t     id;
	int         is_bot;
	const char *username;
	const char *first_name;
	const char *last_name;
} TgUser;


/* Chat */
enum {
	TG_CHAT_TYPE_PRIVATE,
	TG_CHAT_TYPE_GROUP,
	TG_CHAT_TYPE_SUPERGROUP,
	TG_CHAT_TYPE_CHANNEL,
	TG_CHAT_TYPE_UNKNOWN,
};

typedef struct {
	int64_t     id;
	int         type;
	int         is_forum;
	const char *title;
	const char *username;
	const char *first_name;
	const char *last_name;
} TgChat;

const char *tg_chat_type_str(int type);


/*
 * Message
 */
enum {
	TG_MESSAGE_ENTITY_TYPE_MENTION,
	TG_MESSAGE_ENTITY_TYPE_HASHTAG,
	TG_MESSAGE_ENTITY_TYPE_CASHTAG,
	TG_MESSAGE_ENTITY_TYPE_BOT_CMD,
	TG_MESSAGE_ENTITY_TYPE_URL,
	TG_MESSAGE_ENTITY_TYPE_EMAIL,
	TG_MESSAGE_ENTITY_TYPE_PHONE,
	TG_MESSAGE_ENTITY_TYPE_SPOILER,
	TG_MESSAGE_ENTITY_TYPE_TEXT_BOLD,
	TG_MESSAGE_ENTITY_TYPE_TEXT_ITALIC,
	TG_MESSAGE_ENTITY_TYPE_TEXT_UNDERLINE,
	TG_MESSAGE_ENTITY_TYPE_TEXT_BLOCKQUOTE,
	TG_MESSAGE_ENTITY_TYPE_TEXT_BLOCKQUOTE_X,
	TG_MESSAGE_ENTITY_TYPE_TEXT_CODE,
	TG_MESSAGE_ENTITY_TYPE_TEXT_PRE,
	TG_MESSAGE_ENTITY_TYPE_TEXT_LINK,
	TG_MESSAGE_ENTITY_TYPE_TEXT_MENTION,
	TG_MESSAGE_ENTITY_TYPE_CUSTOM_EMOJI,
	TG_MESSAGE_ENTITY_TYPE_UNKNOWN,
};

typedef struct {
	int     type;
	int64_t offset;
	int64_t length;
	union {
		const char *url;		/* text_link only */
		const char *lang;		/* 'pre' only */
		TgUser     *user;		/* text_mention only */
		int64_t     custom_emoji_id;	/* custom_emoji only */
	};
} TgMessageEntity;

const char *tg_entity_type_str(int type);


enum {
	TG_MESSAGE_TYPE_AUDIO,
	TG_MESSAGE_TYPE_DOCUMENT,
	TG_MESSAGE_TYPE_VIDEO,
	TG_MESSAGE_TYPE_TEXT,
	TG_MESSAGE_TYPE_PHOTO,
	TG_MESSAGE_TYPE_UNKNOWN,
};

typedef struct tg_message TgMessage;

typedef struct {
	const char *id;
	const char *uid;
	const char *perfomer;
	const char *title;
	const char *file_name;
	const char *mime_type;
	int64_t     duration;
	int64_t     file_size;
} TgMessageAudio;

typedef struct {
	const char *id;
	const char *uid;
	const char *file_name;
	const char *mime_type;
	int64_t     file_size;
} TgMessageDocument;

typedef struct {
	const char *id;
	const char *uid;
	const char *file_name;
	const char *mime_type;
	int64_t     file_size;
} TgMessagePhotoSize;

typedef struct {
	const char *id;
	const char *uid;
	const char *file_name;
	const char *mime_type;
	int64_t     width;
	int64_t     height;
	int64_t     duration;
	int64_t     file_size;
} TgMessageVideo;

typedef struct tg_message {
	int              type;
	int64_t          id;
	int64_t          date;
	TgUser           from;
	TgChat           chat;
	TgMessage       *reply_to;
	size_t           entities_len;
	TgMessageEntity *entities;
	const char      *caption;
	union {
		const char         *text;
		TgMessageAudio      audio;
		TgMessageDocument   document;
		TgMessagePhotoSize *photos;	/* "NULL-terminated" array */
		TgMessageVideo      video;
	};
} TgMessage;

const char *tg_message_type_str(int type);


/*
 * Update
 */
typedef struct {
	int       has_message;
	int64_t   id;
	TgMessage message;
} TgUpdate;

int  tg_update_parse(TgUpdate *u, json_object *json);
void tg_update_free(TgUpdate *u);


/*
 * Api
 */
typedef struct {
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
