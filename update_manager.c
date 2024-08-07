#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <json.h>
#include <string.h>

#include "update_manager.h"

#include "tg.h"
#include "tg_api.h"


static void _handle_message(UpdateManager *u, const TgMessage *t, json_object *json_obj);


/*
 * Public
 */
int
update_manager_init(UpdateManager *u, int64_t owner_id, const char base_api[], Db *db)
{
	int ret = tg_api_init(&u->api, base_api);
	if (ret < 0)
		return -1;

	ret = module_init(&u->module, owner_id, &u->api, db);
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
		module_handle_callback_query(&u->module, &update.callback_query, json_obj);
		break;
	case TG_UPDATE_TYPE_INLINE_QUERY:
		module_handle_inline_query(&u->module, &update.inline_query, json_obj);
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
	if (t->from == NULL)
		return;

	log_debug("update_manager: _handle: message type: %s", tg_message_type_str(t->type));
	switch (t->type) {
	case TG_MESSAGE_TYPE_COMMAND:
		module_handle_command(&u->module, t, json_obj);
		break;
	case TG_MESSAGE_TYPE_TEXT:
		module_handle_text(&u->module, t, json_obj);
		break;
	default:
		break;
	}
}
