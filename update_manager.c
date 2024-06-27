#include <errno.h>
#include <inttypes.h>
#include <json.h>
#include <string.h>

#include "update_manager.h"
#include "tg.h"
#include "tg_api.h"

#include "builtin/general.h"
#include "builtin/anime_schedule.h"


static void _handle(UpdateManager *u, const TgMessage *t);
static void _handle_command(UpdateManager *u, const TgMessage *t);
static void _handle_text_plain(UpdateManager *u, const TgMessage *t);

#ifdef DEBUG
static void _dump_message(const TgMessage *t);
#endif


/*
 * Public
 */
int
update_manager_init(UpdateManager *u, const char base_api[])
{
	int ret = str_init_alloc(&u->str, 1024);
	if (ret < 0) {
		log_err(ret, "update_manager: update_manager_init: str_init_alloc");
		return -1;
	}

	ret = tg_api_init(&u->api, base_api);
	if (ret < 0) {
		str_deinit(&u->str);
		return ret;
	}

	return 0;
}


void
update_manager_deinit(UpdateManager *u)
{
	tg_api_deinit(&u->api);
	str_deinit(&u->str);
}


int
update_manager_handle(UpdateManager *u, json_object *json)
{
	int ret = -1;

	TgUpdate update;
	if (tg_update_parse(&update, json) < 0)
		goto out0;

	log_info("update id: %" PRIi64, update.id);
	if (update.has_message == 0)
		goto out0;

#ifdef DEBUG
	_dump_message(&update.message);
#endif

	_handle(u, &update.message);
	ret = 0;

out0:
	json_object_put(json);
	return ret;
}


/*
 * Private
 */
static void
_handle(UpdateManager *u, const TgMessage *t)
{
	if (t->entities != NULL) {
		if (t->entities->type == TG_MESSAGE_ENTITY_TYPE_BOT_CMD) {
			_handle_command(u, t);
			return;
		}
	}

	if (t->type == TG_MESSAGE_TYPE_TEXT) {
		_handle_text_plain(u, t);
		return;
	}
}


static void
_handle_text_plain(UpdateManager *u, const TgMessage *t)
{
	(void)u;
	log_debug("text: %s", t->text.text);
}


static void
_handle_command(UpdateManager *u, const TgMessage *t)
{
	size_t len;
	const char *args;
	const char *const text = t->text.text;


	const char *const cmd_end = strchr(text, ' ');
	if (cmd_end != NULL) {
		len = (cmd_end - text);
		args = cmd_end + 1;
	} else {
		len = strlen(text);
		args = "";
	}

	if (cstr_cmp_n("/start", text, len) == 0)
		general_start(&u->api, t, args);
	else if (cstr_cmp_n("/help", text, len) == 0)
		general_help(&u->api, t, args);
	else if (cstr_cmp_n("/anime_schedule", text, len) == 0)
		anime_schedule(&u->api, t, args);

	/* TODO: call external module */
}


#ifdef DEBUG
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
#endif
