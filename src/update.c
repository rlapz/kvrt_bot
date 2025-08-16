#include <assert.h>
#include <stdint.h>
#include <strings.h>

#include "update.h"

#include "cmd.h"
#include "tg.h"
#include "util.h"


static void _handle_message(const Update *u, const TgMessage *msg);
static void _handle_callback(const Update *u, const TgCallbackQuery *cb);
static void _handle_message_command(const Update *u, const TgMessage *msg);
static void _handle_member_new(const Update *u, const TgMessage *msg);
static void _handle_member_leave(const Update *u, const TgMessage *msg);
static void _admin_load(const TgMessage *msg);


/*
 * Update
 */
void
update_handle(const Update *u)
{
	LOG_INFO("update", "%s", json_object_to_json_string_ext(u->resp, JSON_C_TO_STRING_PRETTY));

	TgUpdate tgu;
	if (tg_update_parse(&tgu, u->resp) < 0) {
		LOG_ERRN("update", "%s", "tg_update_parse: failed");
		return;
	}

	switch (tgu.type) {
	case TG_UPDATE_TYPE_MESSAGE:
		_handle_message(u, &tgu.message);
		break;
	case TG_UPDATE_TYPE_CALLBACK_QUERY:
		_handle_callback(u, &tgu.callback_query);
		break;
	}

	tg_update_free(&tgu);
}


/*
 * Private
 */
static void
_handle_message(const Update *u, const TgMessage *msg)
{
	LOG_DEBUG("update", "%zu", msg->id);
	if (msg->from == NULL) {
		LOG_ERRN("update", "%s", "\"message from\" == NULL");
		return;
	}

	switch (msg->type) {
	case TG_MESSAGE_TYPE_COMMAND:
		_handle_message_command(u, msg);
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
_handle_callback(const Update *u, const TgCallbackQuery *cb)
{
	LOG_DEBUG("update", "%s", "update: _handle_callback");
	if (VAL_IS_NULL_OR(cb->message, cb->data)) {
		LOG_ERRN("update", "%s", "(TgMessage or CallbackData) == NULL");
		return;
	}

	if (model_chat_init(cb->message->chat.id) < 0) {
		send_text_plain(cb->message, NULL, "Failed to initialize chat");
		return;
	}

	CmdParam param = {
		.id_bot = u->id_bot,
		.id_owner = u->id_owner,
		.id_user = cb->from.id,
		.id_chat = cb->message->chat.id,
		.id_message = cb->message->id,
		.id_callback = cb->id,
		.bot_username = u->username,
		.msg = cb->message,
		.json = u->resp,
	};

	cmd_exec(&param, cb->data);
}


static void
_handle_message_command(const Update *u, const TgMessage *msg)
{
	LOG_DEBUG("update", "%s", "");
	if (cstr_is_empty(msg->text.cstr)) {
		LOG_ERRN("update", "%s", "Text is empty/NULL");
		return;
	}

	if (model_chat_init(msg->chat.id) < 0) {
		send_text_plain(msg, NULL, "Failed to initialize chat");
		return;
	}

	CmdParam param = {
		.id_bot = u->id_bot,
		.id_owner = u->id_owner,
		.id_user = msg->from->id,
		.id_chat = msg->chat.id,
		.id_message = msg->id,
		.bot_username = u->username,
		.msg = msg,
		.json = u->resp,
	};

	cmd_exec(&param, msg->text.cstr);
}


static void
_handle_member_new(const Update *u, const TgMessage *msg)
{
	LOG_DEBUG("update", "%s", "");

	const int64_t chat_id = msg->chat.id;
	const TgUser *const user = &msg->new_member;
	if (user->id == u->id_bot) {
		_admin_load(msg);
		model_chat_init(chat_id);
		return;
	}

	if (model_admin_get_privileges(chat_id, u->id_bot) > 0) {
		const ModelSchedMessage schd = {
			.type = MODEL_SCHED_MESSAGE_TYPE_DELETE,
			.chat_id = chat_id,
			.message_id = msg->id,
			.user_id = user->id,
			.expire = 5,
		};

		if (model_sched_message_add(&schd, 3) <= 0)
			delete_message(msg);
	}

	if (user->is_bot)
		return;

	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return;

	char *const fname_e = tg_escape(user->first_name);
	if (fname_e == NULL)
		goto out0;

	if (str_set_fmt(&str, "Hello [%s](tg://user?id=%" PRIi64 ") \\:3", fname_e, user->id) == NULL)
		goto out1;

	int64_t ret_id;
	if (send_text_format(msg, &ret_id, "%s", str.cstr) < 0)
		goto out1;

	const ModelSchedMessage schd = {
		.type = MODEL_SCHED_MESSAGE_TYPE_DELETE,
		.chat_id = chat_id,
		.message_id = ret_id,
		.user_id = user->id,
		.expire = 5
	};

	model_sched_message_add(&schd, 10);

out1:
	free(fname_e);
out0:
	str_deinit(&str);
}


static void
_handle_member_leave(const Update *u, const TgMessage *msg)
{
	LOG_DEBUG("update", "%s", "");
	if (msg->left_chat_member.id == u->id_bot)
		return;

	if (model_admin_get_privileges(msg->chat.id, u->id_bot) <= 0)
		return;

	const ModelSchedMessage schd = {
		.type = MODEL_SCHED_MESSAGE_TYPE_DELETE,
		.chat_id = msg->chat.id,
		.message_id = msg->id,
		.user_id = msg->left_chat_member.id,
		.expire = 5,
	};

	if (model_sched_message_add(&schd, 3) <= 0)
		delete_message(msg);
}


static void
_admin_load(const TgMessage *msg)
{
	TgChatAdminList admin_list;
	const int64_t chat_id = msg->chat.id;
	TgApiResp resp = { .udata = &admin_list };
	char buff[1024];

	const int ret = tg_api_get_admin_list(chat_id, &resp);
	if (ret == TG_API_RESP_ERR_API) {
		SEND_ERROR_TEXT(msg, NULL, "%s", "tg_api_get_admin_list: %s",
				tg_api_resp_str(&resp, buff, LEN(buff)));
		return;
	} else if (ret < 0) {
		SEND_ERROR_TEXT(msg, NULL, "%s", "tg_api_get_admin_list: errnum: %d", ret);
		return;
	}

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
	tg_chat_admin_list_free(&admin_list);
}
