#include <errno.h>

#include "update.h"

#include "cmd/common.h"
#include "cmd/builtin.h"


static int  _cmd_compare(const char cmd[], const BotCmd *src);
static void _handle_message(Update *u, const TgMessage *msg, json_object *json);
static void _handle_text(Update *u, const TgMessage *msg, json_object *json);
static int  _handle_commands(Update *u, const TgMessage *msg, json_object *json);
static void _handler_new_member(Update *u, const TgMessage *msg);
static void _admin_load(Update *u, const TgMessage *msg);


/*
 * Public
 */
int
update_init(Update *u, int64_t bot_id, int64_t owner_id, const char base_api[], const char db_path[],
	    Chld *chld)
{
	if (db_init(&u->db, db_path) < 0)
		return -1;

	if (tg_api_init(&u->api, base_api) < 0)
		goto err0;

	if (str_init_alloc(&u->str, 1024) < 0) {
		log_err(ENOMEM, "update: update_init: str_init_alloc");
		goto err1;
	}

	u->bot_id = bot_id;
	u->owner_id = owner_id;
	u->base_api = base_api;
	u->chld = chld;
	return 0;

err1:
	tg_api_deinit(&u->api);
err0:
	db_deinit(&u->db);
	return -1;
}


void
update_deinit(Update *u)
{
	str_deinit(&u->str);
	tg_api_deinit(&u->api);
	db_deinit(&u->db);
}


void
update_handle(Update *u, json_object *json)
{
	TgUpdate update;
	if (tg_update_parse(&update, json) < 0) {
		json_object_put(json);
		return;
	}

	log_debug("update: update_handle: %s", json_object_to_json_string_ext(json, JSON_C_TO_STRING_PRETTY));
	switch (update.type) {
	case TG_UPDATE_TYPE_MESSAGE:
		_handle_message(u, &update.message, json);
		break;
	case TG_UPDATE_TYPE_CALLBACK_QUERY:
		/* TODO */
		break;
	default:
		break;
	}

	tg_update_free(&update);
	json_object_put(json);
}


/*
 * Private
 */
static inline int
_cmd_compare(const char cmd[], const BotCmd *src)
{
	return cstr_casecmp_n(cmd, src->name, (size_t)src->name_len);
}


static void
_handle_message(Update *u, const TgMessage *msg, json_object *json)
{
	if (msg->from == NULL)
		return;

	log_debug("update: _handle_message: message type: %s", tg_message_type_str(msg->type));
	switch (msg->type) {
	case TG_MESSAGE_TYPE_COMMAND:
		if (_handle_commands(u, msg, json))
			break;

		common_cmd_invalid(u, msg);
		break;
	case TG_MESSAGE_TYPE_TEXT:
		_handle_text(u, msg, json);
		break;
	case TG_MESSAGE_TYPE_NEW_MEMBER:
		_handler_new_member(u, msg);
		break;
	default:
		break;
	}
}


static void
_handle_text(Update *u, const TgMessage *msg, json_object *json)
{
	(void)u;
	(void)msg;
	(void)json;
}


static int
_handle_commands(Update *u, const TgMessage *msg, json_object *json)
{
	BotCmd cmd;
	if (bot_cmd_parse(&cmd, '/', msg->text.text) < 0)
		return 0;

	for (unsigned i = 0; ; i++) {
		const CmdBuiltin *const builtin = &cmd_builtins[i];
		if (builtin->name == NULL)
			break;

		if (_cmd_compare(builtin->name, &cmd)) {
			if (builtin->func != NULL)
				builtin->func(u, msg, &cmd, json);
			else
				common_send_todo(u, msg);

			return 1;
		}
	}

	/* TODO: call external command */

	if (common_cmd_message(u, msg, &cmd))
		return 1;

	return 0;
}


static void
_handler_new_member(Update *u, const TgMessage *msg)
{
	const TgUser *const user = &msg->new_member;
	if (user->id == u->bot_id)
		_admin_load(u, msg);
}


static void
_admin_load(Update *u, const TgMessage *msg)
{
	json_object *json_obj;
	TgChatAdminList admin_list;
	int64_t chat_id = msg->chat.id;
	if (tg_api_get_admin_list(&u->api, chat_id, &admin_list, &json_obj) < 0)
		return;

	DbAdmin db_admins[TG_CHAT_ADMIN_LIST_SIZE];
	const int db_admins_len = (int)admin_list.len;
	for (int i = 0; (i < db_admins_len) && (i < TG_CHAT_ADMIN_LIST_SIZE); i++) {
		const TgChatAdmin *const adm = &admin_list.list[i];
		db_admins[i] = (DbAdmin) {
			.chat_id = chat_id,
			.user_id = adm->user->id,
			.is_creator = adm->is_creator,
			.privileges = adm->privileges,
		};
	}

	/* TODO: use transaction */
	db_admin_clear(&u->db, chat_id);
	db_admin_set(&u->db, db_admins, db_admins_len);

	json_object_put(json_obj);
	tg_chat_admin_list_free(&admin_list);
}
