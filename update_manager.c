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
