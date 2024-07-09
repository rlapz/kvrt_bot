#include <errno.h>
#include <inttypes.h>
#include <json.h>
#include <string.h>

#include "update_manager.h"
#include "tg.h"
#include "tg_api.h"


static void _handle(UpdateManager *u, const TgMessage *t, json_object *json_obj);
static void _handle_command(UpdateManager *u, const TgMessage *t, json_object *json_obj);
static void _dump_message(const TgMessage *t);


/*
 * Public
 */
int
update_manager_init(UpdateManager *u, const char base_api[])
{
	int ret = tg_api_init(&u->api, base_api);
	if (ret < 0)
		return -1;

	ret = module_init(&u->module, &u->api);
	if (ret < 0) {
		tg_api_deinit(&u->api);
		return -1;
	}

	return 0;
}


void
update_manager_deinit(UpdateManager *u)
{
	tg_api_deinit(&u->api);
	module_deinit(&u->module);
}


int
update_manager_handle(UpdateManager *u, json_object *json_obj)
{
	int ret = -1;

	TgUpdate update;
	if (tg_update_parse(&update, json_obj) < 0)
		goto out0;

	log_info("update id: %" PRIi64, update.id);
	if (update.has_message == 0)
		goto out0;

#ifdef DEBUG
	_dump_message(&update.message);
#else
	(void)_dump_message;
#endif

	_handle(u, &update.message, json_obj);
	ret = 0;

out0:
	json_object_put(json_obj);
	return ret;
}


/*
 * Private
 */
static void
_handle(UpdateManager *u, const TgMessage *t, json_object *json_obj)
{
	if ((t->entities != NULL) && (t->entities->type == TG_MESSAGE_ENTITY_TYPE_BOT_CMD)) {
		_handle_command(u, t, json_obj);
		return;
	}

	if (t->type == TG_MESSAGE_TYPE_TEXT)
		module_builtin_handle_text(&u->module, t, t->text.text);
}


static void
_handle_command(UpdateManager *u, const TgMessage *t, json_object *json_obj)
{
	size_t len;
	const char *args;
	char command[1024];
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

	cstr_copy_n2(command, sizeof(command), text, len);

	str_reset(&module->str, 0);
	module_builtin_handle_command(module, command, t, json_obj, args);

	/* TODO: call external module */
}


static void
_dump_message(const TgMessage *t)
{
	log_debug("msg_id: %" PRIi64, t->id);
	log_debug("message type: %s", tg_message_type_str(t->type));
	log_debug("entities len: %zu", t->entities_len);
	log_debug("date  : %" PRIi64, t->date);
	log_debug("chat id: %" PRIi64, t->chat.id);
	log_debug("chat type: %s", tg_chat_type_str(t->chat.type));

	const TgMessageEntity *const ent = t->entities;
	for (size_t i = 0; i < t->entities_len; i++) {
		log_debug("ent type: %s", tg_message_entity_type_str(ent[i].type));
		log_debug("offt    : %" PRIi64, ent[i].offset);
		log_debug("length  : %" PRIi64, ent[i].length);

		switch (ent[i].type) {
		case TG_MESSAGE_ENTITY_TYPE_TEXT_PRE:
			log_debug("lang: %s", ent[i].lang);
			break;
		case TG_MESSAGE_ENTITY_TYPE_TEXT_MENTION:
			log_debug("user: TODO");
			break;
		case TG_MESSAGE_ENTITY_TYPE_TEXT_LINK:
			log_debug("url: %s", ent[i].url);
			break;
		case TG_MESSAGE_ENTITY_TYPE_CUSTOM_EMOJI:
			log_debug("emoji: %s", ent[i].custom_emoji_id);
			break;
		}
	}

	const TgUser *const usr = t->from;
	if (usr != NULL) {
		log_debug("from: id: %" PRIi64, usr->id);
		log_debug("from: is_bot: %d", usr->is_bot);
		if (usr->username != NULL)
			log_debug("from: username: %s", usr->username);
		if (usr->first_name != NULL)
			log_debug("from: first_name: %s", usr->first_name);
		if (usr->last_name != NULL)
			log_debug("from: last_name: %s", usr->last_name);
		log_debug("from: is_premium: %d", usr->is_premium);

	}

	const TgMessage *const rpl = t->reply_to;
	if (rpl != NULL) {
		log_debug("reply_to: msg_id: %" PRIi64, rpl->id);
		log_debug("reply_to: message type: %s", tg_message_type_str(rpl->type));
		log_debug("reply_to: entities len: %zu", rpl->entities_len);
		log_debug("reply_to: date  : %" PRIi64, rpl->date);
		log_debug("reply_to: chat id: %" PRIi64, rpl->chat.id);
		log_debug("reply_to: chat type: %s", tg_chat_type_str(rpl->chat.type));
	}

	const TgChat *const cht = &t->chat;
	const char *str = cht->title;
	if (str != NULL)
		log_debug("title: %s", str);

	str = cht->username;
	if (str != NULL)
		log_debug("username: %s", str);

	str = cht->first_name;
	if (str != NULL)
		log_debug("first_name: %s", str);

	str = cht->last_name;
	if (str != NULL)
		log_debug("last_name: %s", str);

	log_debug("is_forum: %d", cht->is_forum);
	switch (t->type) {
	case TG_MESSAGE_TYPE_TEXT:
		log_debug("text: %s", t->text.text);
		break;
	default:
		break;
	}

	if (t->type == TG_MESSAGE_TYPE_AUDIO) {
		const TgAudio *const au = &t->audio;
		log_debug("audio: id       : %s", au->id);
		log_debug("audio: uid      : %s", au->uid);
		log_debug("audio: name     : %s", au->name);
		log_debug("audio: title    : %s", au->title);
		log_debug("audio: performer: %s", au->perfomer);
		log_debug("audio: mime type: %s", au->mime_type);
		log_debug("audio: duration : %" PRIi64, au->size);
		log_debug("audio: size     : %" PRIi64, au->size);
	}

	if (t->type == TG_MESSAGE_TYPE_VIDEO) {
		const TgVideo *const vid = &t->video;
		log_debug("video: id       : %s", vid->id);
		log_debug("video: uid      : %s", vid->uid);
		log_debug("video: name     : %s", vid->name);
		log_debug("video: mime type: %s", vid->mime_type);
		log_debug("video: width    : %" PRIi64, vid->width);
		log_debug("video: height   : %" PRIi64, vid->height);
		log_debug("video: duration : %" PRIi64, vid->duration);
		log_debug("video: size     : %" PRIi64, vid->size);
	}

	if (t->type == TG_MESSAGE_TYPE_DOCUMENT) {
		const TgDocument *const doc = &t->document;
		log_debug("document: id       : %s", doc->id);
		log_debug("document: uid      : %s", doc->uid);
		log_debug("document: name     : %s", doc->name);
		log_debug("document: mime type: %s", doc->mime_type);
		log_debug("document: size     : %" PRIi64, doc->size);
	}

	if (t->type == TG_MESSAGE_TYPE_PHOTO) {
		for (size_t i = 0; ; i++) {
			const TgPhotoSize *const ph = &t->photo[i];
			if (ph->id == NULL)
				break;

			log_debug("photo: id    : %s", ph->id);
			log_debug("photo: uid   : %s", ph->uid);
			log_debug("photo: width : %" PRIi64, ph->width);
			log_debug("photo: height: %" PRIi64, ph->height);
			log_debug("photo: size  : %" PRIi64, ph->size);
		}
	}

	if (t->type == TG_MESSAGE_TYPE_STICKER) {
		const TgSticker *const stc = &t->sticker;
		log_debug("sticker: id           : %s", stc->id);
		log_debug("sticker: uid          : %s", stc->uid);
		log_debug("sticker: name         : %s", stc->name);
		log_debug("sticker: width        : %" PRIi64, stc->width);
		log_debug("sticker: height       : %" PRIi64, stc->height);
		log_debug("sticker: size         : %" PRIi64, stc->size);
		log_debug("sticker: is_animated  : %d", stc->is_animated);
		log_debug("sticker: is_video     : %d", stc->is_video);
		log_debug("sticker: emoji        : %s", stc->emoji);
		log_debug("sticker: name         : %s", stc->name);
		log_debug("sticker: type         : %s", tg_sticker_type_str(stc->type));
		log_debug("sticker: cust emoji id: %s", stc->custom_emoji_id);
		if (stc->thumbnail != NULL) {
			const TgPhotoSize *const ps = stc->thumbnail;
			log_debug("sticker thumb: id    : %s", ps->id);
			log_debug("sticker thumb: uid   : %s", ps->uid);
			log_debug("sticker thumb: width : %" PRIi64, stc->width);
			log_debug("sticker thumb: height: %" PRIi64, stc->height);
			log_debug("sticker thumb: size  : %" PRIi64, stc->size);
		}
	}
}
