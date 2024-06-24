#ifndef __TG_H__
#define __TG_H__


#include <stddef.h>
#include <stdint.h>
#include <json.h>

#include <curl/curl.h>

#include "util.h"


/* User */
typedef struct tg_user TgUser;
struct tg_user {
	int64_t     id;
	int         is_bot;
	int         is_premium;
	const char *username;
	const char *first_name;
	const char *last_name;
};


/* Chat */
enum {
	TG_CHAT_TYPE_PRIVATE,
	TG_CHAT_TYPE_GROUP,
	TG_CHAT_TYPE_SUPERGROUP,
	TG_CHAT_TYPE_CHANNEL,
	TG_CHAT_TYPE_UNKNOWN,
};

typedef struct tg_chat TgChat;
struct tg_chat {
	int64_t     id;
	int         type;
	int         is_forum;
	const char *title;
	const char *username;
	const char *first_name;
	const char *last_name;
};

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
	TG_MESSAGE_ENTITY_TYPE_TEXT_STRIKETHROUGH,
	TG_MESSAGE_ENTITY_TYPE_TEXT_BLOCKQUOTE,
	TG_MESSAGE_ENTITY_TYPE_TEXT_BLOCKQUOTE_X,
	TG_MESSAGE_ENTITY_TYPE_TEXT_CODE,
	TG_MESSAGE_ENTITY_TYPE_TEXT_PRE,
	TG_MESSAGE_ENTITY_TYPE_TEXT_LINK,
	TG_MESSAGE_ENTITY_TYPE_TEXT_MENTION,
	TG_MESSAGE_ENTITY_TYPE_CUSTOM_EMOJI,
	TG_MESSAGE_ENTITY_TYPE_UNKNOWN,
};

typedef struct tg_message_entity TgMessageEntity;
struct tg_message_entity {
	int     type;
	int64_t offset;
	int64_t length;
	union {
		const char *url;		/* text_link only */
		const char *lang;		/* 'pre' only */
		TgUser     *user;		/* text_mention only */
		const char *custom_emoji_id;	/* custom_emoji only */
	};
};

const char *tg_message_entity_type_str(int type);


enum {
	TG_MESSAGE_TYPE_AUDIO,
	TG_MESSAGE_TYPE_DOCUMENT,
	TG_MESSAGE_TYPE_VIDEO,
	TG_MESSAGE_TYPE_TEXT,
	TG_MESSAGE_TYPE_PHOTO,
	TG_MESSAGE_TYPE_UNKNOWN,
};

typedef struct tg_message_audio TgMessageAudio;
struct tg_message_audio {
	const char *id;
	const char *uid;
	const char *name;
	const char *perfomer;
	const char *title;
	const char *mime_type;
	int64_t     duration;
	int64_t     size;
};

typedef struct tg_message_document TgMessageDocument;
struct tg_message_document {
	const char *id;
	const char *uid;
	const char *name;
	const char *mime_type;
	int64_t     size;
};

typedef struct tg_message_photo_size TgMessagePhotoSize;
struct tg_message_photo_size {
	const char *id;
	const char *uid;
	int64_t     width;
	int64_t     height;
	int64_t     size;
};

typedef struct tg_message_video TgMessageVideo;
struct tg_message_video {
	const char *id;
	const char *uid;
	const char *name;
	const char *mime_type;
	int64_t     width;
	int64_t     height;
	int64_t     duration;
	int64_t     size;
};

typedef struct tg_message TgMessage;
struct tg_message {
	int              type;
	int64_t          id;
	int64_t          date;
	TgUser          *from;
	TgChat           chat;
	TgMessage       *reply_to;
	TgMessageEntity *entities;
	size_t           entities_len;
	const char      *caption;
	union {
		const char         *text;
		TgMessageAudio      audio;
		TgMessageDocument   document;
		TgMessagePhotoSize *photo;	/* "NULL-terminated" array */
		TgMessageVideo      video;
	};
};

const char *tg_message_type_str(int type);


/*
 * Update
 */
typedef struct tg_update TgUpdate;
struct tg_update {
	int       has_message;
	int64_t   id;
	TgMessage message;
};

int  tg_update_parse(TgUpdate *u, json_object *json);
void tg_update_free(TgUpdate *u);


#endif
