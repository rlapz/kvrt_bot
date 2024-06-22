#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "tg.h"
#include "config.h"
#include "util.h"


#define _COMPOSE_FMT(PTR, RET, ...)\
	do {									\
		RET = 0;							\
		str_reset(&PTR->str_compose, PTR->api_offt);			\
		if (str_append_fmt(&PTR->str_compose, __VA_ARGS__) == NULL)	\
			RET = -errno;						\
	} while (0)


static int    _parse_message(TgMessage *m, json_object *message_obj);
static void   _parse_message_type(TgMessage *m, json_object *message_obj);
static void   _parse_message_type_audio(TgMessage *m, json_object *audio_obj);
static void   _parse_message_type_document(TgMessage *m, json_object *doc_obj);
static void   _parse_message_type_video(TgMessage *m, json_object *video_obj);
static void   _parse_message_type_text(TgMessage *m, json_object *text_obj);
static void   _parse_message_type_photo(TgMessage *m, json_object *photo_obj);
static void   _parse_message_entities(TgMessage *m, json_object *message_obj);
static int    _parse_chat(TgChat *c, json_object *chat_obj);
static int    _api_curl_init(TgApi *t);
static void   _api_curl_deinit(TgApi *t);
static size_t _api_curl_writer_fn(char buffer[], size_t size, size_t nitems, void *udata);
static int    _api_curl_request_get(TgApi *t, const char url[]);


/*
 * Public
 */
const char *
tg_chat_type_str(int type)
{
	switch (type) {
	case TG_CHAT_TYPE_PRIVATE: return "private";
	case TG_CHAT_TYPE_GROUP: return "group";
	case TG_CHAT_TYPE_SUPERGROUP: return "supergroup";
	case TG_CHAT_TYPE_CHANNEL: return "channel";
	}

	return "unknown";
}


const char *
tg_entity_type_str(int type)
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
	}

	return "unknown";
}


const char *
tg_message_type_str(int type)
{
	switch (type) {
	case TG_MESSAGE_TYPE_AUDIO: return "audio";
	case TG_MESSAGE_TYPE_DOCUMENT: return "document";
	case TG_MESSAGE_TYPE_VIDEO: return "video";
	case TG_MESSAGE_TYPE_TEXT: return "text";
	case TG_MESSAGE_TYPE_PHOTO: return "photo";
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
	
	u->id = json_object_get_int64(obj);

	/* optional */
	if (json_object_object_get_ex(json, "message", &obj) == 0)
		return 0;
	/* optional */

	u->has_message = 1;
	return _parse_message(&u->message, obj);
}


void
tg_update_free(TgUpdate *u)
{
	free(u->message.entities);
	free(u->message.reply_to);
}


int
tg_api_init(TgApi *t, const char base_api[])
{
	int ret = str_init_alloc(&t->str_compose, 1024);
	if (ret < 0) {
		log_err(ret, "tg_api: tg_api_init: str_init_alloc");
		return ret;
	}

	if (str_set_n(&t->str_compose, base_api, strlen(base_api)) == NULL) {
		log_err(errno, "tg_api: tg_api_init: str_set_n: base api: |%s|", base_api);
		goto err0;
	}

	ret = str_init_alloc(&t->str_response, 1024);
	if (ret < 0) {
		log_err(ret, "tg_api: tg_api_init: str_init_alloc: str_response");
		goto err0;
	}

	if (_api_curl_init(t) < 0)
		goto err1;

	t->api = base_api;
	t->api_offt = t->str_compose.len;
	return 0;

err1:
	str_deinit(&t->str_response);
err0:
	str_deinit(&t->str_compose);
	return -1;
}


void
tg_api_deinit(TgApi *t)
{
	str_deinit(&t->str_response);
	str_deinit(&t->str_compose);
	_api_curl_deinit(t);
}


int
tg_api_send_text_plain(TgApi *t, const char chat_id[], const char reply_to[], const char text[])
{
	int ret;
	char *const e_text = curl_easy_escape(t->curl, text, 0);
	if (e_text == NULL)
		return -1;

	_COMPOSE_FMT(t, ret, "sendMessage?chat_id=%s&reply_to_message_id=%s&text=%s",
		     chat_id, reply_to, e_text);
	if (ret < 0) {
		log_err(ret, "tg_api: tg_api_send_text_plain: _COMPOSE_FMT");
		goto out0;
	}

	ret = _api_curl_request_get(t, t->str_compose.cstr);

out0:
	free(e_text);
	return ret;
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
		_parse_message_type_audio(m, obj);
		return;
	}

	if (json_object_object_get_ex(message_obj, "document", &obj) != 0) {
		_parse_message_type_document(m, obj);
		return;
	}

	if (json_object_object_get_ex(message_obj, "video", &obj) != 0) {
		_parse_message_type_video(m, obj);
		return;
	}

	if (json_object_object_get_ex(message_obj, "text", &obj) != 0) {
		_parse_message_type_text(m, obj);
		return;
	}
	
	if (json_object_object_get_ex(message_obj, "photo", &obj) != 0) {
		_parse_message_type_photo(m, obj);
		return;
	}
	
	m->type = TG_MESSAGE_TYPE_UNKNOWN;
}


static void
_parse_message_type_audio(TgMessage *m, json_object *audio_obj)
{
	/* TODO */
	m->type = TG_MESSAGE_TYPE_AUDIO;
}


static void
_parse_message_type_document(TgMessage *m, json_object *doc_obj)
{
	/* TODO */
	m->type = TG_MESSAGE_TYPE_DOCUMENT;
}


static void
_parse_message_type_video(TgMessage *m, json_object *video_obj)
{
	/* TODO */
	m->type = TG_MESSAGE_TYPE_VIDEO;
}


static void
_parse_message_type_text(TgMessage *m, json_object *text_obj)
{
	m->type = TG_MESSAGE_TYPE_TEXT;
	m->text = json_object_get_string(text_obj);
}


static void
_parse_message_type_photo(TgMessage *m, json_object *photo_obj)
{
	/* TODO */
	m->type = TG_MESSAGE_TYPE_PHOTO;
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
	//Entity *const ents = malloc(sizeof(Entity) * len);
	//if (ents == NULL)
	//	return;
	
	//for (size_t i = 0; i < len; i++) {
	//	Entity *const e = &ents[i];
	//}
	
	m->entities_len = len;
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
	
	/* optionals */
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
	/* optionals */

	return 0;
}


static int
_api_curl_init(TgApi *t)
{
	CURL *const curl = curl_easy_init();
	if (curl == NULL) {
		log_err(0, "tg_api: _curl_init: curl_easy_init: failed to init");
		return -1;
	}

	t->curl = curl;
	return 0;
}


static void
_api_curl_deinit(TgApi *t)
{
	curl_easy_cleanup(t->curl);
}


static size_t
_api_curl_writer_fn(char buffer[], size_t size, size_t nitems, void *udata)
{
	const size_t real_size = size * nitems;
	if (str_append_n((Str *)udata, buffer, real_size) == NULL) {
		log_err(errno, "tg_api: _curl_writer_fn: str_append_n: failed to append");
		return 0;
	}

	return real_size;
}


static int
_api_curl_request_get(TgApi *t, const char url[])
{
	curl_easy_reset(t->curl);
	curl_easy_setopt(t->curl, CURLOPT_URL, url);
	curl_easy_setopt(t->curl, CURLOPT_WRITEFUNCTION, _api_curl_writer_fn);

	str_reset(&t->str_response, 0);
	curl_easy_setopt(t->curl, CURLOPT_WRITEDATA, &t->str_response);

	const CURLcode res = curl_easy_perform(t->curl);
	if (res != CURLE_OK) {
		log_err(0, "tg_api: _curl_request_get: curl_easy_perform: %s", curl_easy_strerror(res));
		return -1;
	}

	log_debug("tg_api: _curl_request_get: response: \n---\n%s\n---", t->str_response.cstr);
	return 0;
}
