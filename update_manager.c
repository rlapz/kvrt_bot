#include <errno.h>
#include <inttypes.h>
#include <json.h>
#include <string.h>

#include "update_manager.h"
#include "tg.h"
#include "tg_api.h"


static void _handle_message(UpdateManager *u, const TgMessage *t, json_object *json_obj);
static void _handle_callback_query(UpdateManager *u, const TgCallbackQuery *c, json_object *json_obj);
static void _handle_inline_query(UpdateManager *u, const TgInlineQuery *i, json_object *json_obj);
static void _handle_command(UpdateManager *u, const TgMessage *t, json_object *json_obj);
static void _dump_message(const TgMessage *t);
static void _dump_callback_query(const TgCallbackQuery *c);
static void _dump_inline_query(const TgInlineQuery *i);


/*
 * Public
 */
int
update_manager_init(UpdateManager *u, const char base_api[], Db *db)
{
	int ret = tg_api_init(&u->api, base_api);
	if (ret < 0)
		return -1;

	ret = module_init(&u->module, &u->api, db);
	if (ret < 0) {
		tg_api_deinit(&u->api);
		return -1;
	}

	u->db = db;
	return 0;
}


void
update_manager_deinit(UpdateManager *u)
{
	tg_api_deinit(&u->api);
	module_deinit(&u->module);
}


void
update_manager_handle(UpdateManager *u, json_object *json_obj)
{
	TgUpdate update;
	if (tg_update_parse(&update, json_obj) < 0) {
		json_object_put(json_obj);
		return;
	}

	log_debug("json: %s", json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PRETTY));

	switch (update.type) {
	case TG_UPDATE_TYPE_MESSAGE:
		_handle_message(u, &update.message, json_obj);
		break;
	case TG_UPDATE_TYPE_CALLBACK_QUERY:
		_handle_callback_query(u, &update.callback_query, json_obj);
		break;
	case TG_UPDATE_TYPE_INLINE_QUERY:
		_handle_inline_query(u, &update.inline_query, json_obj);
		break;
	default:
		break;
	}

	tg_update_free(&update);
	json_object_put(json_obj);
}


/*
 * Private
 */
static void
_handle_message(UpdateManager *u, const TgMessage *t, json_object *json_obj)
{
#ifdef DEBUG
	_dump_message(t);
#else
	(void)_dump_message;
#endif

	if (t->from == NULL)
		return;

	log_debug("update_manager: _handle: message type: %s", tg_message_type_str(t->type));
	switch (t->type) {
	case TG_MESSAGE_TYPE_COMMAND:
		_handle_command(u, t, json_obj);
		break;
	case TG_MESSAGE_TYPE_TEXT:
		module_handle_text(&u->module, t, json_obj);
		break;
	case TG_MESSAGE_TYPE_AUDIO:
	case TG_MESSAGE_TYPE_PHOTO:
	case TG_MESSAGE_TYPE_VIDEO:
		module_handle_media(&u->module, t, json_obj);
		break;
	default:
		break;
	}
}


static void
_handle_callback_query(UpdateManager *u, const TgCallbackQuery *c, json_object *json_obj)
{
#ifdef DEBUG
	_dump_callback_query(c);
#else
	(void)_dump_callback_query;
#endif

	/* TODO */
	(void)u;
	(void)c;
	(void)json_obj;
}


static void
_handle_inline_query(UpdateManager *u, const TgInlineQuery *i, json_object *json_obj)
{
#ifdef DEBUG
	_dump_inline_query(i);
#else
	(void)_dump_inline_query;
#endif

	/* TODO */
	(void)u;
	(void)i;
	(void)json_obj;
}


static void
_handle_command(UpdateManager *u, const TgMessage *t, json_object *json_obj)
{
	size_t len;
	const char *args;
	char command[64];
	const char *const text = t->text.text;
	Module *const module = &u->module;


	const char *const cmd_end = strchr(text, ' ');
	if (cmd_end != NULL) {
		len = (cmd_end - text);
		args = cmd_end + 1;
	} else {
		len = strlen(text);
		args = "";
	}

	if (len >= sizeof(command))
		return;

	cstr_copy_n2(command, sizeof(command), text, len);
	module_handle_command(module, command, t, json_obj, args);
}


/*
 * Debug
 */
static void
_dump_message(const TgMessage *t)
{
	log_info("msg_id: %" PRIi64, t->id);
	log_info("message type: %s", tg_message_type_str(t->type));
	log_info("entities len: %zu", t->entities_len);
	log_info("date  : %" PRIi64, t->date);
	log_info("chat id: %" PRIi64, t->chat.id);
	log_info("chat type: %s", tg_chat_type_str(t->chat.type));

	const TgMessageEntity *const ent = t->entities;
	for (size_t i = 0; i < t->entities_len; i++) {
		log_info("ent type: %s", tg_message_entity_type_str(ent[i].type));
		log_info("offt    : %" PRIi64, ent[i].offset);
		log_info("length  : %" PRIi64, ent[i].length);

		switch (ent[i].type) {
		case TG_MESSAGE_ENTITY_TYPE_TEXT_PRE:
			log_info("lang: %s", ent[i].lang);
			break;
		case TG_MESSAGE_ENTITY_TYPE_TEXT_MENTION:
			log_info("user: TODO");
			break;
		case TG_MESSAGE_ENTITY_TYPE_TEXT_LINK:
			log_info("url: %s", ent[i].url);
			break;
		case TG_MESSAGE_ENTITY_TYPE_CUSTOM_EMOJI:
			log_info("emoji: %s", ent[i].custom_emoji_id);
			break;
		default:
			break;
		}
	}

	const TgUser *const usr = t->from;
	if (usr != NULL) {
		log_info("from: id: %" PRIi64, usr->id);
		log_info("from: is_bot: %d", usr->is_bot);
		if (usr->username != NULL)
			log_info("from: username: %s", usr->username);
		if (usr->first_name != NULL)
			log_info("from: first_name: %s", usr->first_name);
		if (usr->last_name != NULL)
			log_info("from: last_name: %s", usr->last_name);
		log_info("from: is_premium: %d", usr->is_premium);

	}

	const TgMessage *const rpl = t->reply_to;
	if (rpl != NULL) {
		log_info("reply_to: msg_id: %" PRIi64, rpl->id);
		log_info("reply_to: message type: %s", tg_message_type_str(rpl->type));
		log_info("reply_to: entities len: %zu", rpl->entities_len);
		log_info("reply_to: date  : %" PRIi64, rpl->date);
		log_info("reply_to: chat id: %" PRIi64, rpl->chat.id);
		log_info("reply_to: chat type: %s", tg_chat_type_str(rpl->chat.type));
	}

	const TgChat *const cht = &t->chat;
	const char *str = cht->title;
	if (str != NULL)
		log_info("title: %s", str);

	str = cht->username;
	if (str != NULL)
		log_info("username: %s", str);

	str = cht->first_name;
	if (str != NULL)
		log_info("first_name: %s", str);

	str = cht->last_name;
	if (str != NULL)
		log_info("last_name: %s", str);

	log_info("is_forum: %d", cht->is_forum);
	switch (t->type) {
	case TG_MESSAGE_TYPE_TEXT:
		log_info("text: %s", t->text.text);
		break;
	default:
		break;
	}

	if (t->type == TG_MESSAGE_TYPE_AUDIO) {
		const TgAudio *const au = &t->audio;
		log_info("audio: id       : %s", au->id);
		log_info("audio: uid      : %s", au->uid);
		log_info("audio: name     : %s", au->name);
		log_info("audio: title    : %s", au->title);
		log_info("audio: performer: %s", au->perfomer);
		log_info("audio: mime type: %s", au->mime_type);
		log_info("audio: duration : %" PRIi64, au->size);
		log_info("audio: size     : %" PRIi64, au->size);
	}

	if (t->type == TG_MESSAGE_TYPE_VIDEO) {
		const TgVideo *const vid = &t->video;
		log_info("video: id       : %s", vid->id);
		log_info("video: uid      : %s", vid->uid);
		log_info("video: name     : %s", vid->name);
		log_info("video: mime type: %s", vid->mime_type);
		log_info("video: width    : %" PRIi64, vid->width);
		log_info("video: height   : %" PRIi64, vid->height);
		log_info("video: duration : %" PRIi64, vid->duration);
		log_info("video: size     : %" PRIi64, vid->size);
	}

	if (t->type == TG_MESSAGE_TYPE_DOCUMENT) {
		const TgDocument *const doc = &t->document;
		log_info("document: id       : %s", doc->id);
		log_info("document: uid      : %s", doc->uid);
		log_info("document: name     : %s", doc->name);
		log_info("document: mime type: %s", doc->mime_type);
		log_info("document: size     : %" PRIi64, doc->size);
	}

	if (t->type == TG_MESSAGE_TYPE_PHOTO) {
		for (size_t i = 0; ; i++) {
			const TgPhotoSize *const ph = &t->photo[i];
			if (ph->id == NULL)
				break;

			log_info("photo: id    : %s", ph->id);
			log_info("photo: uid   : %s", ph->uid);
			log_info("photo: width : %" PRIi64, ph->width);
			log_info("photo: height: %" PRIi64, ph->height);
			log_info("photo: size  : %" PRIi64, ph->size);
		}
	}

	if (t->type == TG_MESSAGE_TYPE_STICKER) {
		const TgSticker *const stc = &t->sticker;
		log_info("sticker: id           : %s", stc->id);
		log_info("sticker: uid          : %s", stc->uid);
		log_info("sticker: name         : %s", stc->name);
		log_info("sticker: width        : %" PRIi64, stc->width);
		log_info("sticker: height       : %" PRIi64, stc->height);
		log_info("sticker: size         : %" PRIi64, stc->size);
		log_info("sticker: is_animated  : %d", stc->is_animated);
		log_info("sticker: is_video     : %d", stc->is_video);
		log_info("sticker: emoji        : %s", stc->emoji);
		log_info("sticker: name         : %s", stc->name);
		log_info("sticker: type         : %s", tg_sticker_type_str(stc->type));
		log_info("sticker: cust emoji id: %s", stc->custom_emoji_id);
		if (stc->thumbnail != NULL) {
			const TgPhotoSize *const ps = stc->thumbnail;
			log_info("sticker thumb: id    : %s", ps->id);
			log_info("sticker thumb: uid   : %s", ps->uid);
			log_info("sticker thumb: width : %" PRIi64, stc->width);
			log_info("sticker thumb: height: %" PRIi64, stc->height);
			log_info("sticker thumb: size  : %" PRIi64, stc->size);
		}
	}
}


static void
_dump_callback_query(const TgCallbackQuery *c)
{
	/* TODO */
	(void)c;
}


static void
_dump_inline_query(const TgInlineQuery *i)
{
	log_info("id: %" PRIi64, i->id);

	const TgUser *const usr = i->from;
	log_info("from: id: %" PRIi64, usr->id);
	log_info("from: is_bot: %d", usr->is_bot);
	if (usr->username != NULL)
		log_info("from: username: %s", usr->username);
	if (usr->first_name != NULL)
		log_info("from: first_name: %s", usr->first_name);
	if (usr->last_name != NULL)
		log_info("from: last_name: %s", usr->last_name);
	log_info("from: is_premium: %d", usr->is_premium);

	log_info("offset: %s", i->offset);
	log_info("query: %s", i->query);
	log_info("chat type: %s", tg_chat_type_str(i->chat_type));
}
