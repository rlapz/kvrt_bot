#include <errno.h>

#include <update.h>
#include <model.h>
#include <module.h>
#include <service.h>
#include <common.h>


static void _handle_message(Update *u, const TgMessage *msg, json_object *json);
static void _handle_text(Update *u, const TgMessage *msg, json_object *json);
static int  _handle_commands(Update *u, const TgMessage *msg, json_object *json);
static void _handle_callback(Update *u, const TgCallbackQuery *cb);
static void _handler_new_member(Update *u, const TgMessage *msg);
static void _admin_load(Update *u, const TgMessage *msg);
static void _send_invalid_cmd(Update *u, const TgMessage *msg);


/*
 * Public
 */
int
update_init(Update *u, int64_t bot_id, int64_t owner_id, const char base_api[], const char db_path[],
	    Chld *chld)
{
	if (service_init(&u->service, db_path) < 0)
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
	service_deinit(&u->service);
	return -1;
}


void
update_deinit(Update *u)
{
	str_deinit(&u->str);
	tg_api_deinit(&u->api);
	service_deinit(&u->service);
}


void
update_handle(Update *u, json_object *json)
{
	TgUpdate update;
	if (tg_update_parse(&update, json) < 0) {
		log_err(0, "update: update_handle: failed to parse Update");
		goto out0;
	}

	log_debug("update: update_handle: %s", json_object_to_json_string_ext(json, JSON_C_TO_STRING_PRETTY));
	switch (update.type) {
	case TG_UPDATE_TYPE_MESSAGE:
		_handle_message(u, &update.message, json);
		break;
	case TG_UPDATE_TYPE_CALLBACK_QUERY:
		_handle_callback(u, &update.callback_query);
		break;
	default:
		break;
	}

	tg_update_free(&update);

out0:
	json_object_put(json);
}


/*
 * Private
 */
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

		_send_invalid_cmd(u, msg);
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
	/* TODO */
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

	if (module_builtin_exec(u, msg, &cmd, json))
		return 1;

	return module_extern_exec(u, msg, &cmd, json);
}


static void
_handle_callback(Update *u, const TgCallbackQuery *cb)
{
	if (cb->data == NULL)
		return;

	(void)u;
}


static void
_handler_new_member(Update *u, const TgMessage *msg)
{
	const TgUser *const user = &msg->new_member;
	if (user->id == u->bot_id) {
		_admin_load(u, msg);
		service_module_extern_setup(&u->service, msg->chat.id);
	}
}


static void
_admin_load(Update *u, const TgMessage *msg)
{
	json_object *json_obj;
	TgChatAdminList admin_list;
	int64_t chat_id = msg->chat.id;
	if (tg_api_get_admin_list(&u->api, chat_id, &admin_list, &json_obj) < 0)
		return;

	Admin db_admins[TG_CHAT_ADMIN_LIST_SIZE];
	const int db_admins_len = (int)admin_list.len;
	for (int i = 0; (i < db_admins_len) && (i < TG_CHAT_ADMIN_LIST_SIZE); i++) {
		const TgChatAdmin *const adm = &admin_list.list[i];
		db_admins[i] = (Admin) {
			.chat_id = chat_id,
			.user_id = adm->user->id,
			.privileges = adm->privileges,
		};
	}

	service_admin_reload(&u->service, db_admins, db_admins_len);

	json_object_put(json_obj);
	tg_chat_admin_list_free(&admin_list);
}


static void
_send_invalid_cmd(Update *u, const TgMessage *msg)
{
	if (msg->chat.type != TG_CHAT_TYPE_PRIVATE)
		return;

	common_send_text_plain(u, msg, str_set_fmt(&u->str, "%s: invalid command!", msg->text));
}
