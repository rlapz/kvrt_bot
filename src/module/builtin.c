#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <module.h>
#include <model.h>
#include <common.h>
#include <service.h>


typedef void (*ModuleBuiltinFn)(Update *u, const ModuleParam *param);

typedef struct module_builti {
	int              flags;
	const char      *name;
	const char      *description;
	ModuleBuiltinFn  handler;
} ModuleBuiltin;


static void _module_cmd_start(Update *u, const ModuleParam *param);
static void _module_cmd_admin_reload(Update *u, const ModuleParam *param);
static void _module_cmd_admin_dump(Update *u, const ModuleParam *param);
static void _module_cmd_dump(Update *u, const ModuleParam *param);
static void _module_cmd_msg_set(Update *u, const ModuleParam *param);
static void _module_cmd_msg_list(Update *u, const ModuleParam *param);
static void _module_cmd_help(Update *u, const ModuleParam *param);
static void _module_cmd_extern_list(Update *u, const ModuleParam *param);
static int  _module_cmd_msg_exec(Update *u, const TgMessage *msg, const BotCmd *cmd);


static const ModuleBuiltin _module_cmd_list[] = {
	{
		.name = "/start",
		.description = "Start",
		.flags = MODULE_FLAG_CMD,
		.handler = _module_cmd_start,
	},
	{
		.name = "/admin_reload",
		.description = "Reload admin list",
		.flags = MODULE_FLAG_CMD | MODULE_FLAG_ADMIN_ONLY,
		.handler = _module_cmd_admin_reload,
	},
	{
		.name = "/admin_dump",
		.description = "Show admin list",
		.flags = MODULE_FLAG_CMD,
		.handler = _module_cmd_admin_dump,
	},
	{
		.name = "/dump",
		.description = "Dump raw JSON message",
		.flags = MODULE_FLAG_CMD,
		.handler = _module_cmd_dump,
	},
	{
		.name = "/msg_set",
		.description = "Set/unset command message",
		.flags = MODULE_FLAG_CMD | MODULE_FLAG_ADMIN_ONLY,
		.handler = _module_cmd_msg_set,
	},
	{
		.name = "/msg_list",
		.description = "Get command message(s)",
		.flags = MODULE_FLAG_CMD | MODULE_FLAG_CALLBACK,
		.handler = _module_cmd_msg_list,
	},
	{
		.name = "/help",
		.description = "Show builtin commands",
		.flags = MODULE_FLAG_CMD,
		.handler = _module_cmd_help,
	},
	{
		.name = "/extern_list",
		.description = "Show external commands",
		.flags = MODULE_FLAG_CMD | MODULE_FLAG_CALLBACK,
		.handler = _module_cmd_extern_list,
	},
};


/*
 * Public
 */
int
module_builtin_exec(Update *update, const ModuleParam *param)
{
	const TgMessage *const msg = param->message;
	for (unsigned i = 0; i < LEN(_module_cmd_list); i++) {
		const ModuleBuiltin *b = &_module_cmd_list[i];
		if (bot_cmd_compare(&param->bot_cmd, b->name) == 0)
			continue;

		assert(b->handler != NULL);
		if ((param->type & b->flags) == 0)
			return 1;

		if ((b->flags & MODULE_FLAG_ADMIN_ONLY) && (common_privileges_check(update, msg) < 0))
			return 1;

		b->handler(update, param);
		return 1;
	}

	if (param->type == MODULE_PARAM_TYPE_CALLBACK)
		return 0;

	return _module_cmd_msg_exec(update, param->message, &param->bot_cmd);
}


/*
 * Private
 */
static void
_module_cmd_start(Update *u, const ModuleParam *param)
{
	if (param->message->chat.type != TG_CHAT_TYPE_PRIVATE)
		return;

	/* TODO */
	common_send_text_plain(u, param->message, "Hello");
}


static void
_module_cmd_admin_reload(Update *u, const ModuleParam *param)
{
	const TgMessage *const msg = param->message;
	const char *resp = "Unknown error!";
	if (msg->chat.type == TG_CHAT_TYPE_PRIVATE) {
		resp = "There are no administrators in the private chat!";
		goto out0;
	}

	const int64_t chat_id = msg->chat.id;
	json_object *json_obj;
	TgChatAdminList admin_list;
	if (tg_api_get_admin_list(&u->api, chat_id, &admin_list, &json_obj) < 0) {
		resp = "Failed to get admin list";
		goto out0;
	}

	int is_priviledged = 0;
	Admin admin_list_e[TG_CHAT_ADMIN_LIST_SIZE];
	const int admin_list_len = (int)admin_list.len;
	for (int i = 0; (i < admin_list_len) && (i < TG_CHAT_ADMIN_LIST_SIZE); i++) {
		const TgChatAdmin *const a = &admin_list.list[i];
		if (msg->from->id == a->user->id) {
			if (a->privileges == 0)
				break;

			is_priviledged = 1;
		}

		admin_list_e[i] = (Admin) {
			.chat_id = chat_id,
			.user_id = a->user->id,
			.privileges = a->privileges,
		};
	}

	if (is_priviledged == 0) {
		resp = "Permission denied!";
		goto out1;
	}

	if (service_admin_reload(&u->service, admin_list_e, admin_list_len) < 0)
		goto out1;

	resp = str_set_fmt(&u->str, "Done! %d admin(s) loaded", admin_list_len);

out1:
	json_object_put(json_obj);
	tg_chat_admin_list_free(&admin_list);
out0:
	common_send_text_plain(u, msg, resp);
}


static void
_module_cmd_admin_dump(Update *u, const ModuleParam *param)
{
	const TgMessage *const msg = param->message;
	if (msg->chat.type == TG_CHAT_TYPE_PRIVATE) {
		common_send_text_plain(u, msg, "There are no administrators in the private chat!");
		return;
	}

	json_object *json_admin_list;
	if (tg_api_get_admin_list(&u->api, msg->chat.id, NULL, &json_admin_list) < 0)
		return;

	ModuleParam _param = {
		.message = msg,
		.json = json_admin_list,
	};

	memcpy(&_param.bot_cmd, &param->bot_cmd, sizeof(param->bot_cmd));
	_module_cmd_dump(u, &_param);
	json_object_put(json_admin_list);
}


static void
_module_cmd_dump(Update *u, const ModuleParam *param)
{
	const TgMessage *const msg = param->message;
	const char *const json_str = json_object_to_json_string_ext(param->json, JSON_C_TO_STRING_PRETTY);
	common_send_text_format(u, msg, str_set_fmt(&u->str, "```json\n%s```", json_str));
}


static void
_module_cmd_msg_set(Update *u, const ModuleParam *param)
{
	const char *resp;
	const TgMessage *const msg = param->message;
	if (msg->chat.type == TG_CHAT_TYPE_PRIVATE) {
		resp = "Not supported!";
		goto out0;
	}

	const BotCmdArg *const args = param->bot_cmd.args;
	const unsigned args_len = param->bot_cmd.args_len;
	if (args_len < 1) {
		resp = "Invalid argument!\nSet: [command_name] message...\nUnset: [command_name] [EMPTY]";
		goto out0;
	}

	char buffer[CMD_MSG_NAME_SIZE];
	const char *name = args[0].name;
	unsigned name_len = args[0].len;
	while (*name != '\0') {
		if (*name != '/')
			break;

		name++;
		name_len--;
	}

	if (name_len == 0) {
		resp = "Invalid command name";
		goto out0;
	}

	if (name_len >= LEN(buffer)) {
		resp = "Command_name too long";
		goto out0;
	}

	const char *msg_text = NULL;
	if (args_len > 1) {
		unsigned msg_len = 0;
		for (unsigned i = 1; i < (args_len - 1); i++)
			msg_len += args[i].len;

		if (msg_len >= CMD_MSG_VALUE_SIZE) {
			resp = "Message too long";
			goto out0;
		}

		msg_text = args[1].name;
	}

	buffer[0] = '/';
	cstr_copy_n2(buffer + 1, LEN(buffer) - 1, name, name_len);

	const CmdMessage cmd_msg = {
		.chat_id = msg->chat.id,
		.created_by = msg->from->id,
		.name_ptr = buffer,
		.message_ptr = msg_text,
	};

	const int ret = service_cmd_message_set(&u->service, &cmd_msg);
	if (ret < 0) {
		resp = "Failed to set command message";
		goto out0;
	}

	if (ret == 0) {
		resp = "No such command message";
		goto out0;
	}

	resp = (msg_text == NULL)? "removed" : "ok";

out0:
	common_send_text_plain(u, msg, resp);
}


static void
_module_cmd_msg_list(Update *u, const ModuleParam *param)
{
	int offt = 0;
	const TgMessage *const msg = param->message;
	if (msg->chat.type == TG_CHAT_TYPE_PRIVATE) {
		common_send_text_plain(u, msg, "Not supported!");
		return;
	}

	CmdMessage msgs[10];
	const int ret = service_cmd_message_get_list(&u->service, msg->chat.id, msgs, LEN(msgs), offt);
	if (ret < 0) {
		common_send_text_plain(u, msg, "Failed to get Command Message list");
		return;
	}

	str_set_fmt(&u->str, "Command Message List:\n");

	char time_buffer[CMD_MSG_CREATED_AT_SIZE * 2];
	for (int i = 0; i < ret; i++) {
		CmdMessage *const m = &msgs[i];
		str_append_fmt(&u->str, "%d\\. %s \\- [%" PRIi64 "](tg://user?id=%" PRIi64 ") \\- %s\n",
			       i + 1, m->name, m->created_by, m->created_by,
			       cstr_tg_escape(time_buffer, m->created_at));
	}

	common_send_text_format(u, msg, u->str.cstr);
}


static void
_module_cmd_help(Update *u, const ModuleParam *param)
{
	str_set_fmt(&u->str, "Builtin commands:\n");
	for (unsigned i = 0, j = 0; i < LEN(_module_cmd_list); i++) {
		const ModuleBuiltin *const b = &_module_cmd_list[i];
		if ((b->name == NULL) || (b->description == NULL) || (b->handler == NULL))
			continue;

		if ((b->flags & MODULE_FLAG_CMD) == 0)
			continue;

		str_append_fmt(&u->str, "%u. %s - %s %s%s\n", ++j , b->name, b->description,
			       ((b->flags & MODULE_FLAG_ADMIN_ONLY)? "🅰️" : ""));
	}

	str_append_fmt(&u->str, "\n---\n🅰️: Admin only");
	common_send_text_plain(u, param->message, u->str.cstr);
}


static void
_module_cmd_extern_list(Update *u, const ModuleParam *param)
{
	common_send_text_plain(u, param->message, "[TODO]");
}


static int
_module_cmd_msg_exec(Update *u, const TgMessage *msg, const BotCmd *cmd)
{
	if (msg->chat.type == TG_CHAT_TYPE_PRIVATE)
		return 0;

	char buffer[CMD_MSG_NAME_SIZE];
	cstr_copy_n2(buffer, LEN(buffer), cmd->name, cmd->name_len);

	CmdMessage cmd_msg = { .chat_id = msg->chat.id, .name_ptr = buffer };
	const int ret = service_cmd_message_get_message(&u->service, &cmd_msg);
	if (ret < 0) {
		common_send_text_plain(u, msg, "Failed to get command message");
		return 1;
	}

	if (ret == 0)
		return 0;

	common_send_text_plain(u, msg, cmd_msg.message);
	return 1;
}
