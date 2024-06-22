#ifndef __UPDATE_H__
#define __UPDATE_H__


#include <stdint.h>
#include <json.h>


enum {
	CHAT_TYPE_PRIVATE,
	CHAT_TYPE_GROUP,
	CHAT_TYPE_CHANNEL,

	CHAT_TYPE_UNKNOWN,
};

enum {
	MESSAGE_TYPE_AUDIO,
	MESSAGE_TYPE_DOCUMENT,
	MESSAGE_TYPE_VIDEO,
	MESSAGE_TYPE_TEXT,
	MESSAGE_TYPE_PHOTO,
	MESSAGE_TYPE_STICKER,

	MESSAGE_TYPE_UNKNOWN,
};

enum {
	ENTITY_TYPE_MENTION,
	ENTITY_TYPE_HASHTAG,
	ENTITY_TYPE_CASHTAG,
	ENTITY_TYPE_BOT_CMD,
	ENTITY_TYPE_URL,
	ENTITY_TYPE_EMAIL,
	ENTITY_TYPE_PHONE,
	ENTITY_TYPE_SPOILER,
	ENTITY_TYPE_TEXT_BOLD,
	ENTITY_TYPE_TEXT_ITALIC,
	ENTITY_TYPE_TEXT_UNDERLINE,
	ENTITY_TYPE_TEXT_BLOCKQUOTE,
	ENTITY_TYPE_TEXT_BLOCKQUOTE_X,
	ENTITY_TYPE_TEXT_CODE,
	ENTITY_TYPE_TEXT_PRE,
	ENTITY_TYPE_TEXT_LINK,
	ENTITY_TYPE_TEXT_MENTION,
	ENTITY_TYPE_CUSTOM_EMOJI,

	ENTITY_TYPE_UNKNOWN,
};


typedef struct {
	int64_t  id;
	int      is_bot;
	char    *first_name;
	char    *last_name;
	char    *username;
} User;

typedef struct {
	int64_t  id;
	int      type;
	int      is_forum;
	char    *title;
	char    *username;
	char    *first_name;
	char    *last_name;
} Chat;

typedef struct {
	int      type;
	int64_t  offset;
	char    *url;
	User    *user; /* Don't free */
	int64_t  custom_emoji_id;
} Entity;

typedef struct {
	char    *id;
	char    *uid;
	char    *perfomer;
	char    *title;
	char    *file_name;
	char    *mime_type;
	int64_t  duration;
	int64_t  file_size;
} MessageAudio;

typedef struct {
	char    *id;
	char    *uid;
	char    *file_name;
	char    *mime_type;
	int64_t  file_size;
} MessageDocument;

typedef struct {
	char    *id;
	char    *uid;
	char    *file_name;
	char    *mime_type;
	int64_t  file_size;
} MessagePhotoSize;

typedef struct {
	char    *id;
	char    *uid;
	char    *file_name;
	char    *mime_type;
	int64_t  width;
	int64_t  height;
	int64_t  duration;
	int64_t  file_size;
} MessageVideo;

typedef struct message Message;
typedef struct message {
	int       type;
	int64_t   id;
	int64_t   thread_id;
	int64_t   date;
	User      from;
	Chat      chat;
	Message   reply_to;
	unsigned  entities_len;
	Entity   *entities;
	union {
		char             *text;
		MessageAudio      audio;
		MessageDocument   document;
		MessagePhotoSize *photos;	/* "NULL-terminated" array */
		MessageVideo      video;
	};
};

typedef struct {
	int64_t id;
	Message message;
} Update;

int  update_parse(Update *u, json_object *json);
void update_free(Update *u);


#endif
