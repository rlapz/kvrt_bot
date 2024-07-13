#ifndef __TG_H__
#define __TG_H__


#include <stddef.h>
#include <stdint.h>
#include <json.h>

#include <curl/curl.h>

#include "util.h"


/* User */
typedef struct tg_user {
	int64_t     id;
	int         is_bot;
	int         is_premium;
	const char *username;
	const char *first_name;
	const char *last_name;
} TgUser;


/* Chat */
typedef enum tg_chat_type {
	TG_CHAT_TYPE_PRIVATE,
	TG_CHAT_TYPE_GROUP,
	TG_CHAT_TYPE_SUPERGROUP,
	TG_CHAT_TYPE_CHANNEL,
	TG_CHAT_TYPE_SENDER,
	TG_CHAT_TYPE_UNKNOWN,
} TgChatType;

typedef struct tg_chat {
	int64_t     id;
	int         type;
	int         is_forum;
	const char *title;
	const char *username;
	const char *first_name;
	const char *last_name;
} TgChat;

const char *tg_chat_type_str(TgChatType type);


/*
 * Text
 */
typedef struct tg_text {
	const char *text;
} TgText;


/*
 * Audio
 */
typedef struct tg_audio {
	const char *id;
	const char *uid;
	const char *name;
	const char *perfomer;
	const char *title;
	const char *mime_type;
	int64_t     duration;
	int64_t     size;
} TgAudio;


/*
 * Document
 */
typedef struct tg_document {
	const char *id;
	const char *uid;
	const char *name;
	const char *mime_type;
	int64_t     size;
} TgDocument;


/*
 * PhotoSize
 */
typedef struct tg_photo_size {
	const char *id;
	const char *uid;
	int64_t     width;
	int64_t     height;
	int64_t     size;
} TgPhotoSize;


/*
 * Video
 */
typedef struct tg_video {
	const char *id;
	const char *uid;
	const char *name;
	const char *mime_type;
	int64_t     width;
	int64_t     height;
	int64_t     duration;
	int64_t     size;
} TgVideo;


/*
 * Sticker
 */
typedef enum tg_sticker_type {
	TG_STICKER_TYPE_REGULAR,
	TG_STICKER_TYPE_MASK,
	TG_STICKER_TYPE_CUSTOM_EMOJI,
	TG_STICKER_TYPE_UNKNWON,
} TgStickerType;

typedef struct tg_sticker {
	TgStickerType  type;
	const char    *id;
	const char    *uid;
	int64_t        width;
	int64_t        height;
	int            is_animated;
	int            is_video;
	TgPhotoSize   *thumbnail;
	const char    *emoji;
	const char    *name;
	const char    *custom_emoji_id;
	int64_t        size;
} TgSticker;

const char * tg_sticker_type_str(TgStickerType type);


/*
 * Message
 */
typedef enum tg_message_entity_type {
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
} TgMessageEntityType;

typedef struct tg_message_entity {
	TgMessageEntityType type;
	int64_t             offset;
	int64_t             length;
	union {
		const char *url;		/* text_link only */
		const char *lang;		/* 'pre' only */
		TgUser     *user;		/* text_mention only */
		const char *custom_emoji_id;	/* custom_emoji only */
	};
} TgMessageEntity;

const char *tg_message_entity_type_str(TgMessageEntityType type);


typedef enum tg_mesage_type {
	TG_MESSAGE_TYPE_AUDIO,
	TG_MESSAGE_TYPE_DOCUMENT,
	TG_MESSAGE_TYPE_VIDEO,
	TG_MESSAGE_TYPE_TEXT,
	TG_MESSAGE_TYPE_PHOTO,
	TG_MESSAGE_TYPE_STICKER,
	TG_MESSAGE_TYPE_COMMAND,
	TG_MESSAGE_TYPE_UNKNOWN,
} TgMessageType;

typedef struct tg_message {
	TgMessageType      type;
	int64_t            id;
	int64_t            date;
	TgUser            *from;
	TgChat             chat;
	struct tg_message *reply_to;
	TgMessageEntity   *entities;
	size_t             entities_len;
	const char        *caption;
	union {
		TgText       text;
		TgAudio      audio;
		TgDocument   document;
		TgPhotoSize *photo;	/* "NULL-terminated" array */
		TgVideo      video;
		TgSticker    sticker;
	};
} TgMessage;

const char *tg_message_type_str(TgMessageType type);


/*
 * Callback Query
 */
typedef struct tg_callback_query {
	int64_t     id;
	TgUser     *from;
	const char *inline_id;
	const char *chat_instance;
	const char *data;
} TgCallbackQuery;


/*
 * Inline Query
 */
typedef struct tg_inline_query {
	int64_t     id;
	TgUser     *from;
	const char *offset;
	const char *query;
	TgChatType  chat_type;
} TgInlineQuery;


/*
 * Update
 */
typedef enum tg_update_type {
	TG_UPDATE_TYPE_MESSAGE,
	TG_UPDATE_TYPE_CALLBACK_QUERY,
	TG_UPDATE_TYPE_INLINE_QUERY,
	TG_UPDATE_TYPE_UNKNOWN,
} TgUpdateType;

const char *tg_update_type_str(TgUpdateType type);

typedef struct tg_update {
	TgUpdateType type;
	int64_t      id;
	union {
		TgMessage       message;
		TgCallbackQuery callback_query;
		TgInlineQuery   inline_query;
	};
} TgUpdate;

int  tg_update_parse(TgUpdate *u, json_object *json);
void tg_update_free(TgUpdate *u);


#endif
