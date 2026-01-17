#include <assert.h>
#include <stdint.h>
#include <strings.h>

#include "update.h"

#include "cmd.h"
#include "sched.h"
#include "tg.h"
#include "util.h"


static void _handle_message(const Update *u, const TgMessage *msg);
static void _handle_callback(const Update *u, const TgCallbackQuery *cb);
static void _handle_message_command(const Update *u, const TgMessage *msg);
static void _handle_member_new(const Update *u, const TgMessage *msg);
static void _handle_member_leave(const Update *u, const TgMessage *msg);
static void _handle_member_state(const TgMessage *msg, const TgUser *user, const char text[]);


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
		SEND_ERROR_TEXT(cb->message, NULL, "%s", "Failed to initialize chat");
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
		SEND_ERROR_TEXT(msg, NULL, "%s", "Failed to initialize chat");
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

	const TgUser *const user = &msg->new_member;
	if (user->id == u->id_bot) {
		admin_reload(msg);
		model_chat_init(msg->chat.id);
		return;
	}

	_handle_member_state(msg, user, "Hello");
}


static void
_handle_member_leave(const Update *u, const TgMessage *msg)
{
	LOG_DEBUG("update", "%s", "");

	_handle_member_state(msg, &msg->left_chat_member, "See you later");
	(void)u;
}


static void
_handle_member_state(const TgMessage *msg, const TgUser *user, const char text[])
{
	const int64_t chat_id = msg->chat.id;
	if (model_admin_get_privileges(chat_id, user->id) > 0) {
		const SchedParam schd = {
			.chat_id = chat_id,
			.message_id = msg->id,
			.user_id = user->id,
			.expire = 5,
			.interval = 3,
		};

		if (sched_delete_message(&schd) <= 0)
			delete_message(msg);
	}

	if (user->is_bot)
		return;

	char *const fname = tg_escape(user->first_name);
	if (fname == NULL)
		return;

	char *const msg_body = cstr_fmt("%s [%s](tg://user?id=%" PRIi64 ") \\:3", text, fname, user->id);
	if (msg_body == NULL)
		goto out0;

	int64_t ret_id;
	if (send_text_format(msg, &ret_id, "%s", msg_body) < 0)
		goto out1;

	const SchedParam schd = {
		.chat_id = msg->chat.id,
		.message_id = ret_id,
		.user_id = user->id,
		.expire = 5,
		.interval = 20,
	};

	sched_delete_message(&schd);

out1:
	free(msg_body);
out0:
	free(fname);
}
