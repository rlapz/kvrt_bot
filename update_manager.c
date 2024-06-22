#include <errno.h>
#include <inttypes.h>
#include <json.h>

#include "update_manager.h"
#include "tg.h"


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

	u->json = NULL;
	return 0;
}


void
update_manager_deinit(UpdateManager *u)
{
	tg_api_deinit(&u->api);
	json_object_put(u->json);
	str_deinit(&u->str);
}


int
update_manager_handle(UpdateManager *u, json_object *json)
{
	if (u->json != NULL)
		json_object_put(u->json);

	log_debug("update: %p: update_handle: \n---\n%s\n---", (void *)u,
		  json_object_to_json_string(json));
	
	// test
	tg_api_send_text_plain(&u->api, "809925732", "",
			       json_object_to_json_string_ext(json, JSON_C_TO_STRING_PRETTY));

	TgUpdate update;
	if (tg_update_parse(&update, json) == 0) {
		log_info("id    : %" PRIi64, update.id);
		if (update.has_message) {
			TgMessage *msg = &update.message;
			log_info("msg_id: %" PRIi64, msg->id);
			log_info("message type: %s", tg_message_type_str(msg->type));
			log_info("entities len: %zu", msg->entities_len);
			log_info("date  : %" PRIi64, msg->date);
			log_info("chat id: %" PRIi64, msg->chat.id);
			log_info("chat type: %s", tg_chat_type_str(msg->chat.type));

			TgMessageEntity *ent = msg->entities;
			for (size_t i = 0; i < msg->entities_len; i++) {
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

			TgUser *usr = msg->from;
			if (usr != NULL) {
				log_info("from: id: %" PRIi64, usr->id);
				log_info("from: is_bot: %d", usr->is_bot);
				if (usr->username != NULL)
					log_info("from: username: %s", usr->username);
				if (usr->first_name != NULL)
					log_info("from: first_name: %s", usr->first_name);
				if (usr->last_name != NULL)
					log_info("from: last_name: %s", usr->last_name);
			}

			TgMessage *rpl = msg->reply_to;
			if (rpl != NULL) {
				log_info("reply_to: msg_id: %" PRIi64, rpl->id);
				log_info("reply_to: message type: %s", tg_message_type_str(rpl->type));
				log_info("reply_to: entities len: %zu", rpl->entities_len);
				log_info("reply_to: date  : %" PRIi64, rpl->date);
				log_info("reply_to: chat id: %" PRIi64, rpl->chat.id);
				log_info("reply_to: chat type: %s", tg_chat_type_str(rpl->chat.type));
			}

			TgChat *cht = &msg->chat;
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
			switch (msg->type) {
			case TG_MESSAGE_TYPE_TEXT:
				log_info("text: %s", msg->text);
				break;
			default:
				break;
			}
		}

		tg_update_free(&update);
	}
	// test

	u->json = json;
	return 0;
}


/*
 * Private
 */
