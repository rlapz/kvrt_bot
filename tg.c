#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "tg.h"

#include "config.h"
#include "util.h"


static int  _parse_message(TgMessage *m, json_object *message_obj);
static int  _parse_message_type(TgMessage *m, json_object *message_obj);
static int  _parse_audio(TgAudio *a, json_object *audio_obj);
static int  _parse_document(TgDocument *d, json_object *doc_obj);
static int  _parse_video(TgVideo *v, json_object *video_obj);
static void _parse_text(TgText *t, json_object *text_obj);
static int  _parse_photo(TgPhotoSize *p, json_object *photo_obj);
static int  _parse_sticker(TgSticker *s, json_object *sticker_obj);
static int  _parse_message_entities(TgMessage *m, json_object *message_obj);
static int  _parse_callback_query(TgCallbackQuery *c, json_object *callback_query_obj);
static int  _pares_inline_query(TgInlineQuery *i, json_object *inline_query_obj);
static int  _parse_user(TgUser **u, json_object *user_obj);
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
	case TG_CHAT_TYPE_SENDER: return "sender";
	default: break;
	}

	return "unknown";
}


TgChatType
tg_chat_type_get(const char type_str[])
{
	if (strcmp(type_str, "private") == 0)
		return TG_CHAT_TYPE_PRIVATE;

	if (strcmp(type_str, "group") == 0)
		return TG_CHAT_TYPE_GROUP;

	if (strcmp(type_str, "supergroup") == 0)
		return TG_CHAT_TYPE_SUPERGROUP;

	if (strcmp(type_str, "channel") == 0)
		return TG_CHAT_TYPE_CHANNEL;

	return TG_CHAT_TYPE_UNKNOWN;
}


const char *
tg_sticker_type_str(TgStickerType type)
{
	switch (type) {
	case TG_STICKER_TYPE_REGULAR: return "regular";
	case TG_STICKER_TYPE_MASK: return "mask";
	case TG_STICKER_TYPE_CUSTOM_EMOJI: return "custom emoji";
	default: break;
	}

	return "unknown";
}


int
tg_chat_admin_parse(TgChatAdmin *a, json_object *json)
{
	TgChatAdminPrivilege privs = 0;
	memset(a, 0, sizeof(*a));

	json_object *user_obj;
	if (json_object_object_get_ex(json, "user", &user_obj) == 0)
		return -1;

	json_object *is_anon_obj;
	if (json_object_object_get_ex(json, "is_anonymous", &is_anon_obj) == 0)
		return -1;

	json_object *can_be_edited_obj;
	if (json_object_object_get_ex(json, "can_be_edited", &can_be_edited_obj) == 0)
		return -1;

	json_object *can_manage_chat_obj;
	if (json_object_object_get_ex(json, "can_manage_chat", &can_manage_chat_obj) == 0)
		return -1;

	json_object *can_delete_messages_obj;
	if (json_object_object_get_ex(json, "can_delete_message", &can_delete_messages_obj) == 0)
		return -1;

	json_object *can_manage_video_chats_obj;
	if (json_object_object_get_ex(json, "can_manage_video_chats", &can_manage_video_chats_obj) == 0)
		return -1;

	json_object *can_restrict_members_obj;
	if (json_object_object_get_ex(json, "can_restrict_members", &can_restrict_members_obj) == 0)
		return -1;

	json_object *can_promote_members_obj;
	if (json_object_object_get_ex(json, "can_promote_members", &can_promote_members_obj) == 0)
		return -1;

	json_object *can_change_info_obj;
	if (json_object_object_get_ex(json, "can_change_info", &can_change_info_obj) == 0)
		return -1;

	json_object *can_invite_users_obj;
	if (json_object_object_get_ex(json, "can_invite_users", &can_invite_users_obj) == 0)
		return -1;

	json_object *can_post_stories_obj;
	if (json_object_object_get_ex(json, "can_post_stories", &can_post_stories_obj) == 0)
		return -1;

	json_object *can_edit_stories_obj;
	if (json_object_object_get_ex(json, "can_edit_stories", &can_edit_stories_obj) == 0)
		return -1;

	json_object *can_delete_stories_obj;
	if (json_object_object_get_ex(json, "can_delete_stories", &can_delete_stories_obj) == 0)
		return -1;


	json_object *obj;
	if (json_object_object_get_ex(json, "can_post_messages", &obj) && json_object_get_boolean(obj))
		privs |= TG_CHAT_ADMIN_PRI_CAN_POST_MESSAGES;
	if (json_object_object_get_ex(json, "can_edit_messages", &obj) && json_object_get_boolean(obj))
		privs |= TG_CHAT_ADMIN_PRI_CAN_EDIT_MESSAGES;
	if (json_object_object_get_ex(json, "can_pin_messages", &obj) && json_object_get_boolean(obj))
		privs |= TG_CHAT_ADMIN_PRI_CAN_POST_MESSAGES;
	if (json_object_object_get_ex(json, "can_manage_topics", &obj) && json_object_get_boolean(obj))
		privs |= TG_CHAT_ADMIN_PRI_CAN_MANAGE_TOPICS;
	if (json_object_object_get_ex(json, "custom_title", &obj))
		a->custom_title = json_object_get_string(obj);
	if (json_object_get_boolean(can_be_edited_obj))
		privs |= TG_CHAT_ADMIN_PRI_CAN_BE_EDITED;


	if (json_object_get_boolean(can_manage_chat_obj))
		privs |= TG_CHAT_ADMIN_PRI_CAN_MANAGE_CHAT;
	if (json_object_get_boolean(can_delete_messages_obj))
		privs |= TG_CHAT_ADMIN_PRI_CAN_DELETE_MESSAGES;
	if (json_object_get_boolean(can_manage_video_chats_obj))
		privs |= TG_CHAT_ADMIN_PRI_CAN_MANAGE_VIDEO_CHATS;
	if (json_object_get_boolean(can_restrict_members_obj))
		privs |= TG_CHAT_ADMIN_PRI_CAN_RESTRICT_MEMBERS;
	if (json_object_get_boolean(can_promote_members_obj))
		privs |= TG_CHAT_ADMIN_PRI_CAN_PROMOTE_MEMBERS;
	if (json_object_get_boolean(can_promote_members_obj))
		privs |= TG_CHAT_ADMIN_PRI_CAN_PROMOTE_MEMBERS;
	if (json_object_get_boolean(can_change_info_obj))
		privs |= TG_CHAT_ADMIN_PRI_CAN_CHANGE_INFO;
	if (json_object_get_boolean(can_invite_users_obj))
		privs |= TG_CHAT_ADMIN_PRI_CAN_INVITE_USERS;
	if (json_object_get_boolean(can_post_stories_obj))
		privs |= TG_CHAT_ADMIN_PRI_CAN_POST_STORIES;
	if (json_object_get_boolean(can_edit_stories_obj))
		privs |= TG_CHAT_ADMIN_PRI_CAN_EDIT_STORIS;
	if (json_object_get_boolean(can_delete_stories_obj))
		privs |= TG_CHAT_ADMIN_PRI_CAN_DELETE_STORIES;

	a->privileges = privs;
	return _parse_user(&a->user, user_obj);
}


void
tg_chat_admin_free(TgChatAdmin *a)
{
	free(a->user);
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
	case TG_MESSAGE_TYPE_STICKER: return "sticker";
	case TG_MESSAGE_TYPE_COMMAND: return "bot command";
	default: break;
	}

	return "unknown";
}


const char *
tg_update_type_str(TgUpdateType type)
{
	switch (type) {
	case TG_UPDATE_TYPE_MESSAGE: return "message";
	case TG_UPDATE_TYPE_INLINE_QUERY: return "inline query";
	default: break;
	}

	return "unknown";
}


int
tg_update_parse(TgUpdate *u, json_object *json)
{
	memset(u, 0, sizeof(*u));
	u->type = TG_UPDATE_TYPE_UNKNOWN;

	json_object *update_id_obj;
	if (json_object_object_get_ex(json, "update_id", &update_id_obj) == 0)
		return -1;

	u->id = json_object_get_int64(update_id_obj);


	json_object *message_obj;
	if (json_object_object_get_ex(json, "message", &message_obj) != 0) {
		if (_parse_message(&u->message, message_obj) < 0)
			return -1;

		json_object *reply_to_obj;
		if (json_object_object_get_ex(message_obj, "reply_to_message", &reply_to_obj) != 0) {
			TgMessage *const reply_to = calloc(1, sizeof(TgMessage));
			if (_parse_message(reply_to, reply_to_obj) == 0)
				u->message.reply_to = reply_to;
			else
				free(reply_to);
		}

		u->type = TG_UPDATE_TYPE_MESSAGE;
		return 0;
	}

	json_object *inline_obj;
	if (json_object_object_get_ex(json, "inline_query", &inline_obj) != 0) {
		if (_pares_inline_query(&u->inline_query, inline_obj) < 0)
			return -1;

		u->type = TG_UPDATE_TYPE_INLINE_QUERY;
		return 0;
	}

	json_object *callback_query_obj;
	if (json_object_object_get_ex(json, "callback_query", &callback_query_obj) != 0) {
		if (_parse_callback_query(&u->callback_query, callback_query_obj) < 0)
			return -1;

		u->type = TG_UPDATE_TYPE_CALLBACK_QUERY;
		return 0;
	}

	u->type = TG_UPDATE_TYPE_UNKNOWN;
	return -1;
}


void
tg_update_free(TgUpdate *u)
{
	switch (u->type) {
	case TG_UPDATE_TYPE_MESSAGE:
		if (u->message.reply_to != NULL)
			_free_message(u->message.reply_to);

		_free_message(&u->message);
		break;
	case TG_UPDATE_TYPE_CALLBACK_QUERY:
		free(u->callback_query.from);
		break;
	case TG_UPDATE_TYPE_INLINE_QUERY:
		free(u->inline_query.from);
		break;
	default:
		break;
	}
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
	if (json_object_object_get_ex(message_obj, "from", &from_obj) != 0) {
		if (_parse_user(&m->from, from_obj) < 0)
			return -1;
	}

	if (_parse_message_type(m, message_obj) < 0)
		goto err0;

	if (_parse_message_entities(m, message_obj) < 0)
		goto err0;

	if ((m->entities != NULL) && (m->entities->type == TG_MESSAGE_ENTITY_TYPE_BOT_CMD))
		m->type = TG_MESSAGE_TYPE_COMMAND;

	if (_parse_chat(&m->chat, chat_obj) < 0)
		goto err0;

	m->id = json_object_get_int64(id_obj);
	m->date = json_object_get_int64(date_obj);
	return 0;

err0:
	_free_message(m);
	return -1;
}


/* TODO */
static int
_parse_message_type(TgMessage *m, json_object *message_obj)
{
	json_object *obj;

	/* guess all possibilities */
	if (json_object_object_get_ex(message_obj, "audio", &obj) != 0) {
		if (_parse_audio(&m->audio, obj) < 0)
			return -1;

		m->type = TG_MESSAGE_TYPE_AUDIO;
		return 0;
	}

	if (json_object_object_get_ex(message_obj, "document", &obj) != 0) {
		if (_parse_document(&m->document, obj) < 0)
			return -1;

		m->type = TG_MESSAGE_TYPE_DOCUMENT;
		return 0;
	}

	if (json_object_object_get_ex(message_obj, "video", &obj) != 0) {
		if (_parse_video(&m->video, obj) < 0)
			return -1;

		m->type = TG_MESSAGE_TYPE_VIDEO;
		return 0;
	}

	if (json_object_object_get_ex(message_obj, "text", &obj) != 0) {
		_parse_text(&m->text, obj);
		m->type = TG_MESSAGE_TYPE_TEXT;
		return 0;
	}

	if (json_object_object_get_ex(message_obj, "photo", &obj) != 0) {
		array_list *const list = json_object_get_array(obj);
		if (list == NULL)
			return -1;

		const size_t len = array_list_length(list) + 1; /* +1: NULL "id" */
		TgPhotoSize *const photo = calloc(len, sizeof(TgPhotoSize));
		if (photo != NULL) {
			for (size_t i = 0, j = 0; j < len; j++) {
				json_object *const _obj = array_list_get_idx(list, j);
				if (_parse_photo(&photo[i], _obj) < 0)
					continue;

				i++;
			}
		}

		m->photo = photo;
		m->type = TG_MESSAGE_TYPE_PHOTO;
		return 0;
	}

	if (json_object_object_get_ex(message_obj, "sticker", &obj) != 0) {
		if (_parse_sticker(&m->sticker, obj) < 0)
			return -1;

		m->type = TG_MESSAGE_TYPE_STICKER;
		return 0;
	}

	m->type = TG_MESSAGE_TYPE_UNKNOWN;
	return 0;
}


static int
_parse_audio(TgAudio *a, json_object *audio_obj)
{
	json_object *id_obj;
	if (json_object_object_get_ex(audio_obj, "file_id", &id_obj) == 0)
		return -1;

	json_object *uid_obj;
	if (json_object_object_get_ex(audio_obj, "file_unique_id", &uid_obj) == 0)
		return -1;

	json_object *duration_obj;
	if (json_object_object_get_ex(audio_obj, "duration", &duration_obj) == 0)
		return -1;

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
	return 0;
}


static int
_parse_document(TgDocument *d, json_object *doc_obj)
{
	json_object *id_obj;
	if (json_object_object_get_ex(doc_obj, "file_id", &id_obj) == 0)
		return -1;

	json_object *uid_obj;
	if (json_object_object_get_ex(doc_obj, "file_unique_id", &uid_obj) == 0)
		return -1;

	json_object *obj;
	if (json_object_object_get_ex(doc_obj, "file_name", &obj) != 0)
		d->name = json_object_get_string(obj);

	if (json_object_object_get_ex(doc_obj, "mime_type", &obj) != 0)
		d->mime_type = json_object_get_string(obj);

	if (json_object_object_get_ex(doc_obj, "file_size", &obj) != 0)
		d->size = json_object_get_int64(obj);

	d->id = json_object_get_string(id_obj);
	d->uid = json_object_get_string(uid_obj);
	return 0;
}


static int
_parse_video(TgVideo *v, json_object *video_obj)
{
	json_object *id_obj;
	if (json_object_object_get_ex(video_obj, "file_id", &id_obj) == 0)
		return -1;

	json_object *uid_obj;
	if (json_object_object_get_ex(video_obj, "file_unique_id", &uid_obj) == 0)
		return -1;

	json_object *duration_obj;
	if (json_object_object_get_ex(video_obj, "duration", &duration_obj) == 0)
		return -1;

	json_object *width_obj;
	if (json_object_object_get_ex(video_obj, "width", &width_obj) == 0)
		return -1;

	json_object *height_obj;
	if (json_object_object_get_ex(video_obj, "height", &height_obj) == 0)
		return -1;

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
	return 0;
}


static void
_parse_text(TgText *t, json_object *text_obj)
{
	t->text = json_object_get_string(text_obj);
}


static int
_parse_photo(TgPhotoSize *p, json_object *photo_obj)
{
	json_object *id_obj;
	if (json_object_object_get_ex(photo_obj, "file_id", &id_obj) == 0)
		return -1;

	json_object *uid_obj;
	if (json_object_object_get_ex(photo_obj, "file_unique_id", &uid_obj) == 0)
		return -1;

	json_object *width_obj;
	if (json_object_object_get_ex(photo_obj, "width", &width_obj) == 0)
		return -1;

	json_object *height_obj;
	if (json_object_object_get_ex(photo_obj, "height", &height_obj) == 0)
		return -1;

	json_object *size_obj;
	if (json_object_object_get_ex(photo_obj, "file_size", &size_obj) != 0)
		p->size = json_object_get_int64(size_obj);

	p->id = json_object_get_string(id_obj);
	p->uid = json_object_get_string(uid_obj);
	p->width = json_object_get_int64(width_obj);
	p->height = json_object_get_int64(height_obj);
	return 0;
}


static int
_parse_sticker(TgSticker *s, json_object *sticker_obj)
{
	json_object *id_obj;
	if (json_object_object_get_ex(sticker_obj, "file_id", &id_obj) == 0)
		return -1;

	json_object *uid_obj;
	if (json_object_object_get_ex(sticker_obj, "file_unique_id", &uid_obj) == 0)
		return -1;

	json_object *type_obj;
	if (json_object_object_get_ex(sticker_obj, "type", &type_obj) == 0)
		return -1;

	json_object *width_obj;
	if (json_object_object_get_ex(sticker_obj, "width", &width_obj) == 0)
		return -1;

	json_object *height_obj;
	if (json_object_object_get_ex(sticker_obj, "height", &height_obj) == 0)
		return -1;

	json_object *is_animated_obj;
	if (json_object_object_get_ex(sticker_obj, "is_animated", &is_animated_obj) == 0)
		return -1;

	json_object *is_video_obj;
	if (json_object_object_get_ex(sticker_obj, "is_video", &is_video_obj) == 0)
		return -1;

	json_object *obj;
	if (json_object_object_get_ex(sticker_obj, "thumbnail", &obj) != 0) {
		TgPhotoSize *thumbn = calloc(1, sizeof(TgPhotoSize));
		if (thumbn != NULL) {
			if (_parse_photo(thumbn, obj) < 0) {
				free(thumbn);
				thumbn = NULL;
			}
		}

		s->thumbnail = thumbn;
	}

	if (json_object_object_get_ex(sticker_obj, "emoji", &obj) != 0)
		s->emoji = json_object_get_string(obj);

	if (json_object_object_get_ex(sticker_obj, "set_name", &obj) != 0)
		s->name = json_object_get_string(obj);

	if (json_object_object_get_ex(sticker_obj, "custom_emoji_id", &obj) != 0)
		s->custom_emoji_id = json_object_get_string(obj);

	if (json_object_object_get_ex(sticker_obj, "file_size", &obj) != 0)
		s->size = json_object_get_int64(obj);

	const char *const type = json_object_get_string(type_obj);
	if (strcmp(type, "regular") == 0)
		s->type = TG_STICKER_TYPE_REGULAR;
	else if (strcmp(type, "mask") == 0)
		s->type = TG_STICKER_TYPE_MASK;
	else if (strcmp(type, "custom_emoji") == 0)
		s->type = TG_STICKER_TYPE_CUSTOM_EMOJI;
	else
		s->type = TG_STICKER_TYPE_UNKNWON;

	s->id = json_object_get_string(id_obj);
	s->uid = json_object_get_string(uid_obj);
	s->width = json_object_get_int64(width_obj);
	s->height = json_object_get_int64(height_obj);
	s->is_animated = json_object_get_boolean(is_animated_obj);
	s->is_video = json_object_get_boolean(is_video_obj);
	return 0;
}


static int
_parse_message_entities(TgMessage *m, json_object *message_obj)
{
	json_object *ents_obj;
	if (json_object_object_get_ex(message_obj, "entities", &ents_obj) == 0)
		return 0;

	array_list *const list = json_object_get_array(ents_obj);
	if (list == NULL)
		return -1;

	const size_t len = array_list_length(list);
	TgMessageEntity *const ents = calloc(len, sizeof(TgMessageEntity));
	if (ents == NULL)
		return -1;

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
	return 0;
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
	c->type = tg_chat_type_get(json_object_get_string(type_obj));
	return 0;
}


static int
_parse_callback_query(TgCallbackQuery *c, json_object *callback_query_obj)
{
	json_object *id_obj;
	if (json_object_object_get_ex(callback_query_obj, "id", &id_obj) == 0)
		return -1;

	json_object *from_obj;
	if (json_object_object_get_ex(callback_query_obj, "from", &from_obj) == 0)
		return -1;

	json_object *chat_instance_obj;
	if (json_object_object_get_ex(callback_query_obj, "chat_instance", &chat_instance_obj) == 0)
		return -1;

	json_object *inline_id;
	if (json_object_object_get_ex(callback_query_obj, "inline_message_id", &inline_id) != 0)
		c->data = json_object_get_string(inline_id);

	json_object *data_obj;
	if (json_object_object_get_ex(callback_query_obj, "data", &data_obj) != 0)
		c->data = json_object_get_string(data_obj);

	c->id = json_object_get_int64(id_obj);
	c->chat_instance = json_object_get_string(chat_instance_obj);
	return _parse_user(&c->from, from_obj);
}


static int
_pares_inline_query(TgInlineQuery *i, json_object *inline_query_obj)
{
	json_object *id_obj;
	if (json_object_object_get_ex(inline_query_obj, "id", &id_obj) == 0)
		return -1;

	json_object *from_obj;
	if (json_object_object_get_ex(inline_query_obj, "from", &from_obj) == 0)
		return -1;

	json_object *query_obj;
	if (json_object_object_get_ex(inline_query_obj, "query", &query_obj) == 0)
		return -1;

	json_object *offset_obj;
	if (json_object_object_get_ex(inline_query_obj, "offset", &offset_obj) == 0)
		return -1;

	json_object *chat_type;
	if (json_object_object_get_ex(inline_query_obj, "chat_type", &chat_type) != 0) {
		const char *const type_str = json_object_get_string(chat_type);
		if (strcmp(type_str, "private") == 0)
			i->chat_type = TG_CHAT_TYPE_PRIVATE;
		else if (strcmp(type_str, "group") == 0)
			i->chat_type = TG_CHAT_TYPE_GROUP;
		else if (strcmp(type_str, "supergroup") == 0)
			i->chat_type = TG_CHAT_TYPE_SUPERGROUP;
		else if (strcmp(type_str, "channel") == 0)
			i->chat_type = TG_CHAT_TYPE_CHANNEL;
		else if (strcmp(type_str, "sender") == 0)
			i->chat_type = TG_CHAT_TYPE_SENDER;
		else
			i->chat_type = TG_CHAT_TYPE_UNKNOWN;
	}

	i->id = json_object_get_int64(id_obj);
	i->query = json_object_get_string(query_obj);
	i->offset = json_object_get_string(offset_obj);
	return _parse_user(&i->from, from_obj);
}


static int
_parse_user(TgUser **u, json_object *user_obj)
{
	json_object *id_obj;
	if (json_object_object_get_ex(user_obj, "id", &id_obj) == 0)
		return -1;

	json_object *is_bot_obj;
	if (json_object_object_get_ex(user_obj, "is_bot", &is_bot_obj) == 0)
		return -1;

	TgUser *const user = calloc(1, sizeof(TgUser));
	if (user == NULL)
		return -1;

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
	return 0;
}


static void
_free_message(TgMessage *m)
{
	if (m->type == TG_MESSAGE_TYPE_PHOTO)
		free(m->photo);

	if (m->type == TG_MESSAGE_TYPE_STICKER)
		free(m->sticker.thumbnail);

	free(m->from);
	free(m->entities);
	free(m->reply_to);
}
