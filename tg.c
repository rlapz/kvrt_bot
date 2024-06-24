#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "tg.h"
#include "config.h"
#include "util.h"


static int  _parse_message(TgMessage *m, json_object *message_obj);
static void _parse_message_type(TgMessage *m, json_object *message_obj);
static void _parse_message_type_audio(TgMessageAudio *a, json_object *audio_obj);
static void _parse_message_type_document(TgMessageDocument *d, json_object *doc_obj);
static void _parse_message_type_video(TgMessageVideo *v, json_object *video_obj);
static void _parse_message_type_text(const char *t[], json_object *text_obj);
static void _parse_message_type_photo(TgMessagePhotoSize *p[], json_object *photo_obj);
static void _parse_message_entities(TgMessage *m, json_object *message_obj);
static void _parse_user(TgUser **u, json_object *user_obj);
static int  _parse_chat(TgChat *c, json_object *chat_obj);
static void _free_message(TgMessage *m);


/*
 * Public
 */
const char *
tg_chat_type_str(TgChatType type)
{
	switch (type) {
	case TG_CHAT_TYPE_PRIVATE: return "private";
	case TG_CHAT_TYPE_GROUP: return "group";
	case TG_CHAT_TYPE_SUPERGROUP: return "supergroup";
	case TG_CHAT_TYPE_CHANNEL: return "channel";
	default: break;
	}

	return "unknown";
}


const char *
tg_message_entity_type_str(TgMessageEntityType type)
{
	switch (type) {
	case TG_MESSAGE_ENTITY_TYPE_MENTION: return "mention";
	case TG_MESSAGE_ENTITY_TYPE_HASHTAG: return "hashtag";
	case TG_MESSAGE_ENTITY_TYPE_CASHTAG: return "cashtag";
	case TG_MESSAGE_ENTITY_TYPE_BOT_CMD: return "bot command";
	case TG_MESSAGE_ENTITY_TYPE_URL: return "url";
	case TG_MESSAGE_ENTITY_TYPE_EMAIL: return "email";
	case TG_MESSAGE_ENTITY_TYPE_PHONE: return "phone number";
	case TG_MESSAGE_ENTITY_TYPE_SPOILER: return "spoiler";
	case TG_MESSAGE_ENTITY_TYPE_TEXT_BOLD: return "text bold";
	case TG_MESSAGE_ENTITY_TYPE_TEXT_ITALIC: return "text italic";
	case TG_MESSAGE_ENTITY_TYPE_TEXT_UNDERLINE: return "text underline";
	case TG_MESSAGE_ENTITY_TYPE_TEXT_BLOCKQUOTE: return "text blockquote";
	case TG_MESSAGE_ENTITY_TYPE_TEXT_BLOCKQUOTE_X: return "text blockquote extended";
	case TG_MESSAGE_ENTITY_TYPE_TEXT_CODE: return "text code";
	case TG_MESSAGE_ENTITY_TYPE_TEXT_PRE: return "text pre";
	case TG_MESSAGE_ENTITY_TYPE_TEXT_LINK: return "text link";
	case TG_MESSAGE_ENTITY_TYPE_TEXT_MENTION: return "text metion";
	case TG_MESSAGE_ENTITY_TYPE_CUSTOM_EMOJI: return "custom emoji";
	default: break;
	}

	return "unknown";
}


const char *
tg_message_type_str(TgMessageType type)
{
	switch (type) {
	case TG_MESSAGE_TYPE_AUDIO: return "audio";
	case TG_MESSAGE_TYPE_DOCUMENT: return "document";
	case TG_MESSAGE_TYPE_VIDEO: return "video";
	case TG_MESSAGE_TYPE_TEXT: return "text";
	case TG_MESSAGE_TYPE_PHOTO: return "photo";
	default: break;
	}

	return "unknown";
}


int
tg_update_parse(TgUpdate *u, json_object *json)
{
	memset(u, 0, sizeof(*u));

	json_object *obj;
	if (json_object_object_get_ex(json, "update_id", &obj) == 0)
		return -1;

	if (json_object_object_get_ex(json, "message", &obj) != 0) {
		if (_parse_message(&u->message, obj) < 0)
			return -1;

		json_object *reply_to_obj;
		if (json_object_object_get_ex(obj, "reply_to_message", &reply_to_obj) != 0) {
			TgMessage *const reply_to = calloc(1, sizeof(TgMessage));
			if (_parse_message(reply_to, reply_to_obj) == 0)
				u->message.reply_to = reply_to;
			else
				free(reply_to);
		}

		u->has_message = 1;
	}

	u->id = json_object_get_int64(obj);
	return 0;
}


void
tg_update_free(TgUpdate *u)
{
	if (u->message.reply_to != NULL)
		_free_message(u->message.reply_to);

	_free_message(&u->message);
}


/*
 * Private
 */
static int
_parse_message(TgMessage *m, json_object *message_obj)
{
	json_object *id_obj;
	if (json_object_object_get_ex(message_obj, "message_id", &id_obj) == 0)
		return -1;

	json_object *date_obj;
	if (json_object_object_get_ex(message_obj, "date", &date_obj) == 0)
		return -1;

	json_object *chat_obj;
	if (json_object_object_get_ex(message_obj, "chat", &chat_obj) == 0)
		return -1;

	json_object *capt_obj;
	if (json_object_object_get_ex(message_obj, "caption", &capt_obj) != 0)
		m->caption = json_object_get_string(capt_obj);

	json_object *from_obj;
	if (json_object_object_get_ex(message_obj, "from", &from_obj) != 0)
		_parse_user(&m->from, from_obj);

	m->id = json_object_get_int64(id_obj);
	m->date = json_object_get_int64(date_obj);

	_parse_message_entities(m, message_obj);
	_parse_message_type(m, message_obj);
	return _parse_chat(&m->chat, chat_obj);
}


static void
_parse_message_type(TgMessage *m, json_object *message_obj)
{
	json_object *obj;

	/* guess all possibilities */
	if (json_object_object_get_ex(message_obj, "audio", &obj) != 0) {
		m->type = TG_MESSAGE_TYPE_AUDIO;
		_parse_message_type_audio(&m->audio, obj);
		return;
	}

	if (json_object_object_get_ex(message_obj, "document", &obj) != 0) {
		m->type = TG_MESSAGE_TYPE_DOCUMENT;
		_parse_message_type_document(&m->document, obj);
		return;
	}

	if (json_object_object_get_ex(message_obj, "video", &obj) != 0) {
		m->type = TG_MESSAGE_TYPE_VIDEO;
		_parse_message_type_video(&m->video, obj);
		return;
	}

	if (json_object_object_get_ex(message_obj, "text", &obj) != 0) {
		m->type = TG_MESSAGE_TYPE_TEXT;
		_parse_message_type_text(&m->text, obj);
		return;
	}

	if (json_object_object_get_ex(message_obj, "photo", &obj) != 0) {
		m->type = TG_MESSAGE_TYPE_PHOTO;
		_parse_message_type_photo(&m->photo, obj);
		return;
	}

	m->type = TG_MESSAGE_TYPE_UNKNOWN;
}


static void
_parse_message_type_audio(TgMessageAudio *a, json_object *audio_obj)
{
	json_object *id_obj;
	if (json_object_object_get_ex(audio_obj, "file_id", &id_obj) == 0)
		return;

	json_object *uid_obj;
	if (json_object_object_get_ex(audio_obj, "file_unique_id", &uid_obj) == 0)
		return;

	json_object *duration_obj;
	if (json_object_object_get_ex(audio_obj, "duration", &duration_obj) == 0)
		return;

	json_object *obj;
	if (json_object_object_get_ex(audio_obj, "file_name", &obj) != 0)
		a->name = json_object_get_string(obj);

	if (json_object_object_get_ex(audio_obj, "title", &obj) != 0)
		a->title = json_object_get_string(obj);

	if (json_object_object_get_ex(audio_obj, "performer", &obj) != 0)
		a->perfomer = json_object_get_string(obj);

	if (json_object_object_get_ex(audio_obj, "mime_type", &obj) != 0)
		a->mime_type = json_object_get_string(obj);

	if (json_object_object_get_ex(audio_obj, "file_size", &obj) != 0)
		a->size = json_object_get_int64(obj);

	a->id = json_object_get_string(id_obj);
	a->uid = json_object_get_string(uid_obj);
	a->duration = json_object_get_int64(duration_obj);
}


static void
_parse_message_type_document(TgMessageDocument *d, json_object *doc_obj)
{
	json_object *id_obj;
	if (json_object_object_get_ex(doc_obj, "file_id", &id_obj) == 0)
		return;

	json_object *uid_obj;
	if (json_object_object_get_ex(doc_obj, "file_unique_id", &uid_obj) == 0)
		return;

	json_object *obj;
	if (json_object_object_get_ex(doc_obj, "file_name", &obj) != 0)
		d->name = json_object_get_string(obj);

	if (json_object_object_get_ex(doc_obj, "mime_type", &obj) != 0)
		d->mime_type = json_object_get_string(obj);

	if (json_object_object_get_ex(doc_obj, "file_size", &obj) != 0)
		d->size = json_object_get_int64(obj);

	d->id = json_object_get_string(id_obj);
	d->uid = json_object_get_string(uid_obj);
}


static void
_parse_message_type_video(TgMessageVideo *v, json_object *video_obj)
{
	json_object *id_obj;
	if (json_object_object_get_ex(video_obj, "file_id", &id_obj) == 0)
		return;

	json_object *uid_obj;
	if (json_object_object_get_ex(video_obj, "file_unique_id", &uid_obj) == 0)
		return;

	json_object *duration_obj;
	if (json_object_object_get_ex(video_obj, "duration", &duration_obj) == 0)
		return;

	json_object *width_obj;
	if (json_object_object_get_ex(video_obj, "width", &width_obj) == 0)
		return;

	json_object *height_obj;
	if (json_object_object_get_ex(video_obj, "height", &height_obj) == 0)
		return;

	json_object *obj;
	if (json_object_object_get_ex(video_obj, "file_name", &obj) != 0)
		v->name = json_object_get_string(obj);

	if (json_object_object_get_ex(video_obj, "mime_type", &obj) != 0)
		v->mime_type = json_object_get_string(obj);

	if (json_object_object_get_ex(video_obj, "file_size", &obj) != 0)
		v->size = json_object_get_int64(obj);

	v->id = json_object_get_string(id_obj);
	v->uid = json_object_get_string(uid_obj);
	v->duration = json_object_get_int64(duration_obj);
	v->width = json_object_get_int64(width_obj);
	v->height = json_object_get_int64(height_obj);
}


static void
_parse_message_type_text(const char *t[], json_object *text_obj)
{
	*t = json_object_get_string(text_obj);
}


static void
_parse_message_type_photo(TgMessagePhotoSize *p[], json_object *photo_obj)
{
	array_list *const list = json_object_get_array(photo_obj);
	if (list == NULL)
		return;

	const size_t len = array_list_length(list) + 1; /* +1: NULL "id" */
	TgMessagePhotoSize *const photo = calloc(len, sizeof(TgMessagePhotoSize));
	if (photo == NULL)
		return;

	for (size_t i = 0, j = 0; j < len; j++) {
		json_object *const obj = array_list_get_idx(list, j);
		TgMessagePhotoSize *const _p = &photo[i];

		json_object *id_obj;
		if (json_object_object_get_ex(obj, "file_id", &id_obj) == 0)
			continue;

		json_object *uid_obj;
		if (json_object_object_get_ex(obj, "file_unique_id", &uid_obj) == 0)
			continue;

		json_object *width_obj;
		if (json_object_object_get_ex(obj, "width", &width_obj) == 0)
			continue;

		json_object *height_obj;
		if (json_object_object_get_ex(obj, "height", &height_obj) == 0)
			continue;

		json_object *size_obj;
		if (json_object_object_get_ex(obj, "file_size", &size_obj) != 0)
			_p->size = json_object_get_int64(size_obj);

		_p->id = json_object_get_string(id_obj);
		_p->uid = json_object_get_string(uid_obj);
		_p->width = json_object_get_int64(width_obj);
		_p->height = json_object_get_int64(height_obj);

		i++;
	}

	*p = photo;
}


static void
_parse_message_entities(TgMessage *m, json_object *message_obj)
{
	json_object *ents_obj;
	if (json_object_object_get_ex(message_obj, "entities", &ents_obj) == 0)
		return;

	array_list *const list = json_object_get_array(ents_obj);
	if (list == NULL)
		return;

	const size_t len = array_list_length(list);
	TgMessageEntity *const ents = calloc(len, sizeof(TgMessageEntity));
	if (ents == NULL)
		return;

	size_t i = 0;
	for (size_t j = 0; j < len; j++) {
		json_object *const obj = array_list_get_idx(list, j);
		json_object *res;
		if (json_object_object_get_ex(obj, "type", &res) == 0)
			continue;

		TgMessageEntity *const e = &ents[i];
		const char *const type = json_object_get_string(res);
		if (strcmp(type, "mention") == 0) {
			e->type = TG_MESSAGE_ENTITY_TYPE_MENTION;
		} else if (strcmp(type, "hashtag") == 0) {
			e->type = TG_MESSAGE_ENTITY_TYPE_HASHTAG;
		} else if (strcmp(type, "cashtag") == 0) {
			e->type = TG_MESSAGE_ENTITY_TYPE_CASHTAG;
		} else if (strcmp(type, "bot_command") == 0) {
			e->type = TG_MESSAGE_ENTITY_TYPE_BOT_CMD;
		} else if (strcmp(type, "url") == 0) {
			e->type = TG_MESSAGE_ENTITY_TYPE_URL;
		} else if (strcmp(type, "email") == 0) {
			e->type = TG_MESSAGE_ENTITY_TYPE_EMAIL;
		} else if (strcmp(type, "phone_number") == 0) {
			e->type = TG_MESSAGE_ENTITY_TYPE_PHONE;
		} else if (strcmp(type, "spoiler") == 0) {
			e->type = TG_MESSAGE_ENTITY_TYPE_SPOILER;
		} else if (strcmp(type, "bold") == 0) {
			e->type = TG_MESSAGE_ENTITY_TYPE_TEXT_BOLD;
		} else if (strcmp(type, "italic") == 0) {
			e->type = TG_MESSAGE_ENTITY_TYPE_TEXT_ITALIC;
		} else if (strcmp(type, "underline") == 0) {
			e->type = TG_MESSAGE_ENTITY_TYPE_TEXT_UNDERLINE;
		} else if (strcmp(type, "strikethrough") == 0) {
			e->type = TG_MESSAGE_ENTITY_TYPE_TEXT_STRIKETHROUGH;
		} else if (strcmp(type, "blockquote") == 0) {
			e->type = TG_MESSAGE_ENTITY_TYPE_TEXT_BLOCKQUOTE;
		} else if (strcmp(type, "expandable_blockquote") == 0) {
			e->type = TG_MESSAGE_ENTITY_TYPE_TEXT_BLOCKQUOTE_X;
		} else if (strcmp(type, "code") == 0) {
			e->type = TG_MESSAGE_ENTITY_TYPE_TEXT_CODE;
		} else if (strcmp(type, "pre") == 0) {
			if (json_object_object_get_ex(obj, "language", &res) != 0)
				e->lang = json_object_get_string(res);

			e->type = TG_MESSAGE_ENTITY_TYPE_TEXT_PRE;
		} else if (strcmp(type, "text_mention") == 0) {
			if (json_object_object_get_ex(obj, "user", &res) != 0)
				_parse_user(&e->user, res);

			e->type = TG_MESSAGE_ENTITY_TYPE_TEXT_MENTION;
		} else if (strcmp(type, "text_link") == 0) {
			if (json_object_object_get_ex(obj, "url", &res) != 0)
				e->url = json_object_get_string(res);

			e->type = TG_MESSAGE_ENTITY_TYPE_TEXT_LINK;
		} else if (strcmp(type, "custom_emoji") == 0) {
			if (json_object_object_get_ex(obj, "custom_emoji_id", &res) != 0)
				e->custom_emoji_id = json_object_get_string(res);

			e->type = TG_MESSAGE_ENTITY_TYPE_CUSTOM_EMOJI;
		} else {
			e->type = TG_MESSAGE_ENTITY_TYPE_UNKNOWN;
		}

		if (json_object_object_get_ex(obj, "offset", &res) == 0)
			continue;

		e->offset = json_object_get_int64(res);

		if (json_object_object_get_ex(obj, "length", &res) == 0)
			continue;

		e->length = json_object_get_int64(res);

		i++;
	}

	m->entities = ents;
	m->entities_len = i;
}


static int
_parse_chat(TgChat *c, json_object *chat_obj)
{
	json_object *id_obj;
	if (json_object_object_get_ex(chat_obj, "id", &id_obj) == 0)
		return -1;

	json_object *type_obj;
	if (json_object_object_get_ex(chat_obj, "type", &type_obj) == 0)
		return -1;

	const char *const type_str = json_object_get_string(type_obj);
	if (strcmp(type_str, "private") == 0)
		c->type = TG_CHAT_TYPE_PRIVATE;
	else if (strcmp(type_str, "group") == 0)
		c->type = TG_CHAT_TYPE_GROUP;
	else if (strcmp(type_str, "supergroup") == 0)
		c->type = TG_CHAT_TYPE_SUPERGROUP;
	else if (strcmp(type_str, "channel") == 0)
		c->type = TG_CHAT_TYPE_CHANNEL;
	else
		c->type = TG_CHAT_TYPE_UNKNOWN;

	json_object *obj;
	if (json_object_object_get_ex(chat_obj, "title", &obj) != 0)
		c->title = json_object_get_string(obj);

	if (json_object_object_get_ex(chat_obj, "username", &obj) != 0)
		c->username = json_object_get_string(obj);

	if (json_object_object_get_ex(chat_obj, "first_name", &obj) != 0)
		c->first_name = json_object_get_string(obj);

	if (json_object_object_get_ex(chat_obj, "last_name", &obj) != 0)
		c->last_name = json_object_get_string(obj);

	if (json_object_object_get_ex(chat_obj, "is_forum", &obj) != 0)
		c->is_forum = json_object_get_boolean(obj);

	c->id = json_object_get_int64(id_obj);
	return 0;
}


static void
_parse_user(TgUser **u, json_object *user_obj)
{
	json_object *id_obj;
	if (json_object_object_get_ex(user_obj, "id", &id_obj) == 0)
		return;

	json_object *is_bot_obj;
	if (json_object_object_get_ex(user_obj, "is_bot", &is_bot_obj) == 0)
		return;

	TgUser *const user = calloc(1, sizeof(TgUser));
	if (user == NULL)
		return;

	json_object *obj;
	if (json_object_object_get_ex(user_obj, "username", &obj) != 0)
		user->username = json_object_get_string(obj);

	if (json_object_object_get_ex(user_obj, "first_name", &obj) != 0)
		user->first_name = json_object_get_string(obj);

	if (json_object_object_get_ex(user_obj, "last_name", &obj) != 0)
		user->last_name = json_object_get_string(obj);

	if (json_object_object_get_ex(user_obj, "is_premium", &obj) != 0)
		user->is_premium = json_object_get_boolean(obj);

	user->id = json_object_get_int64(id_obj);
	user->is_bot = json_object_get_boolean(is_bot_obj);

	*u = user;
}


static void
_free_message(TgMessage *m)
{
	if (m->type == TG_MESSAGE_TYPE_PHOTO)
		free(m->photo);

	free(m->from);
	free(m->entities);
	free(m->reply_to);
}
