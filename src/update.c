#include <strings.h>
#include <stdint.h>

#include "update.h"

#include "cmd.h"
#include "tg.h"
#include "util.h"


static void _handle_message(Update *u, const TgMessage *msg, json_object *json);
static void _handle_callback(Update *u, const TgCallbackQuery *cb, json_object *json);
static void _handle_message_command(Update *u, const TgMessage *msg, json_object *json);
static void _handle_member_new(Update *u, const TgMessage *msg);
static void _handle_member_leave(Update *u, const TgMessage *msg);
static void _admin_load(const TgMessage *msg);


/*
 * Update
 */
void
update_handle(Update *u, json_object *json)
{
	TgUpdate tgu;
	if (tg_update_parse(&tgu, json) < 0) {
		log_err(0, "update: update_handle: tg_update_parse: failed");
		return;
	}

	switch (tgu.type) {
	case TG_UPDATE_TYPE_MESSAGE:
		_handle_message(u, &tgu.message, json);
		break;
	case TG_UPDATE_TYPE_CALLBACK_QUERY:
		_handle_callback(u, &tgu.callback_query, json);
		break;
	}

	tg_update_free(&tgu);
}


/*
 * Private
 */
static void
_handle_message(Update *u, const TgMessage *msg, json_object *json)
{
	log_debug("update: _handle_message: %zu", msg->id);
	if (msg->from == NULL) {
		log_err(0, "update: _handle_message: \"message from\" == NULL");
		return;
	}

	switch (msg->type) {
	case TG_MESSAGE_TYPE_COMMAND:
		_handle_message_command(u, msg, json);
		break;
	case TG_MESSAGE_TYPE_NEW_MEMBER:
		_handle_member_new(u, msg);
		break;
	case TG_MESSAGE_TYPE_LEFT_CHAT_MEMBER:
		_handle_member_leave(u, msg);
		break;
	}
}


static void
_handle_callback(Update *u, const TgCallbackQuery *cb, json_object *json)
{
	log_debug("update: _handle_callback");
	if (VAL_IS_NULL_OR(cb->message, cb->data)) {
		log_err(0, "update: _handle_callback: (TgMessage or CallbackData) == NULL");
		return;
	}

	CmdParam param = {
		.id_bot = u->id_bot,
		.id_owner = u->id_owner,
		.id_user = cb->from.id,
		.id_chat = cb->message->chat.id,
		.id_callback = cb->id,
		.bot_username = u->username,
		.msg = cb->message,
		.json = json,
	};

	cmd_exec(&param, cb->data);
}


static void
_handle_message_command(Update *u, const TgMessage *msg, json_object *json)
{
	log_debug("update: _handle_message_command");
	if (cstr_is_empty(msg->text.cstr)) {
		log_err(0, "update: _handle_message_command: Text is empty/NULL");
		return;
	}

	CmdParam param = {
		.id_bot = u->id_bot,
		.id_owner = u->id_owner,
		.id_user = msg->from->id,
		.id_chat = msg->chat.id,
		.bot_username = u->username,
		.msg = msg,
		.json = json,
	};

	cmd_exec(&param, msg->text.cstr);
}


static void
_handle_member_new(Update *u, const TgMessage *msg)
{
	log_debug("update: _handle_member_new");

	const int64_t chat_id = msg->chat.id;
	const TgUser *const user = &msg->new_member;
	if (user->id == u->id_bot) {
		_admin_load(msg);
		model_cmd_extern_disabled_setup(chat_id);
		return;
	}

	if (model_admin_get_privilegs(chat_id, u->id_bot) > 0) {
		const ModelSchedMessage schd = {
			.type = MODEL_SCHED_MESSAGE_TYPE_DELETE,
			.chat_id = chat_id,
			.message_id = msg->id,
			.expire = 5,
		};

		if (model_sched_message_add(&schd, 3) <= 0)
			tg_api_delete_message(chat_id, msg->id);
	}

	if (user->is_bot)
		return;

	char *const message_e = tg_escape("hello");
	if (message_e == NULL)
		return;

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		goto out0;

	if (str_set_fmt(&str, "%s\n", message_e) == NULL)
		goto out1;

	char *const fname_e = tg_escape(user->first_name);
	if (fname_e == NULL)
		goto out1;

	if (str_append_fmt(&str, "[%s](tg://user?id=%" PRIi64 ")", fname_e, user->id) == NULL)
		goto out2;

	int64_t ret_id;
	if (tg_api_send_text(TG_API_TEXT_TYPE_FORMAT, chat_id, 0, str.cstr, &ret_id) < 0)
		goto out2;

	const ModelSchedMessage schd = {
		.type = MODEL_SCHED_MESSAGE_TYPE_DELETE,
		.chat_id = chat_id,
		.message_id = ret_id,
		.expire = 5
	};

	model_sched_message_add(&schd, 10);

out2:
	free(fname_e);
out1:
	str_deinit(&str);
out0:
	free(message_e);
}


static void
_handle_member_leave(Update *u, const TgMessage *msg)
{
	log_debug("update: _handle_member_leave");
	if (msg->left_chat_member.id == u->id_bot)
		return;

	if (model_admin_get_privilegs(msg->chat.id, u->id_bot) <= 0)
		return;

	const ModelSchedMessage schd = {
		.type = MODEL_SCHED_MESSAGE_TYPE_DELETE,
		.chat_id = msg->chat.id,
		.message_id = msg->id,
		.expire = 5
	};

	if (model_sched_message_add(&schd, 3) <= 0)
		tg_api_delete_message(msg->chat.id, msg->id);
}


static void
_admin_load(const TgMessage *msg)
{
	json_object *json;
	TgChatAdminList admin_list;
	const int64_t chat_id = msg->chat.id;
	if (tg_api_get_admin_list(chat_id, &admin_list, &json) < 0)
		return;

	ModelAdmin db_admin_list[TG_CHAT_ADMIN_LIST_SIZE];
	const int db_admin_list_len = (int)admin_list.len;
	for (int i = 0; (i < db_admin_list_len) && (i < TG_CHAT_ADMIN_LIST_SIZE); i++) {
		const TgChatAdmin *const adm = &admin_list.list[i];
		db_admin_list[i] = (ModelAdmin) {
			.chat_id = chat_id,
			.user_id = adm->user->id,
			.privileges = adm->privileges,
		};
	}

	model_admin_reload(db_admin_list, db_admin_list_len);

	json_object_put(json);
	tg_chat_admin_list_free(&admin_list);
}
