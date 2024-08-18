#include <assert.h>
#include <string.h>

#include "common.h"
#include "builtin.h"

#include "../tg_api.h"
#include "../util.h"


static void _cmd_admin_reload(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json);
static void _cmd_dump(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json);
static void _cmd_dump_admin(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json);
static void _cmd_message_set(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json);
static void _cmd_help(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json);


/*
 * Private
 */
static void
_cmd_admin_reload(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json)
{
	common_admin_reload(u, msg, 1);

	(void)cmd;
	(void)json;
}


static void
_cmd_dump(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json)
{
	const char *const json_str = json_object_to_json_string_ext(json, JSON_C_TO_STRING_PRETTY);
	const char *const resp = str_set_fmt(&u->str, "```json\n%s```", json_str);
	if (resp == NULL) {
		tg_api_send_text(&u->api, TG_API_TEXT_TYPE_PLAIN, msg->chat.id, &msg->id, "failed");
		return;
	}

	tg_api_send_text(&u->api, TG_API_TEXT_TYPE_FORMAT, msg->chat.id, &msg->id, resp);

	(void)cmd;
}


static void
_cmd_dump_admin(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json)
{
	json_object *json_admin_list;
	if (tg_api_get_admin_list(&u->api, msg->chat.id, NULL, &json_admin_list) < 0)
		return;

	_cmd_dump(u, msg, cmd, json_admin_list);
	json_object_put(json_admin_list);

	(void)json;
}


static void
_cmd_message_set(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json)
{
	const char *resp;
	char buffer[DB_CMD_MSG_SIZE];
	if ((msg->chat.type != TG_CHAT_TYPE_PRIVATE) && (common_privileges_check(u, msg) < 0))
		return;

	const BotCmdArg *const args = cmd->args;
	const unsigned args_len = cmd->args_len;
	if (args_len < 1) {
		resp = "invalid argument!\nSet: [command_name] message...\nUnset: [command_name] [EMPTY]";
		goto out0;
	}

	if (args[0].len >= LEN(buffer)) {
		resp = "command_name too long: max: 32 chars";
		goto out0;
	}

	const char *msg_text = NULL;
	if (args_len > 1) {
		unsigned max_msg_len = 0;
		for (unsigned i = 1; i < args_len -1; i++)
			max_msg_len += args[i].len;

		if (max_msg_len >= 8192) {
			resp = "message too long: max: 8192 chars";
			goto out0;
		}

		msg_text = args[1].name;
	}

	buffer[0] = '/';
	cstr_copy_n2(buffer + 1, LEN(buffer) - 1, args[0].name, args[0].len);
	const int ret = db_cmd_message_set(&u->db, msg->chat.id, msg->from->id, buffer, msg_text);
	if (ret < 0) {
		resp = "failed";
		goto out0;
	}

	if (ret == 0) {
		resp = "no such command message";
		goto out0;
	}

	resp = (msg_text == NULL) ? "removed" : "ok";

out0:
	tg_api_send_text(&u->api, TG_API_TEXT_TYPE_PLAIN, msg->chat.id, &msg->id, resp);

	(void)json;
}


static void
_cmd_help(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json)
{
	str_set_fmt(&u->str, "Builtin commands:\n");
	for (unsigned i = 0; ; i++) {
		const CmdBuiltin *const b = &cmd_builtins[i];
		if (b->name == NULL)
			break;

		const char *const icon = (b->admin_only) ? "🅰️" : "";
		str_append_fmt(&u->str, "%u. %s - %s %s\n", i + 1 , b->name, b->description, icon);
	}

	str_append_fmt(&u->str, "\nExternal commands:\n");
	str_append_fmt(&u->str, "[TODO]\n");
	str_append_fmt(&u->str, "\n---\n✅: Enabled - ❎: Disabled - 🅰️: Admin only");

	tg_api_send_text(&u->api, TG_API_TEXT_TYPE_PLAIN, msg->chat.id, &msg->id, u->str.cstr);

	(void)cmd;
	(void)json;
}


/*
 * Public
 */
const CmdBuiltin cmd_builtins[] = {
	{
		.name = "/admin_reload",
		.description = "Reload admin",
		.func = _cmd_admin_reload,
		.admin_only = 1,
	},
	{
		.name = "/dump",
		.description = "Dump raw JSON message",
		.func = _cmd_dump,
	},
	{
		.name = "/dump_admin",
		.description = "Dump admin",
		.func = _cmd_dump_admin,
	},
	{
		.name = "/msg_set",
		.description = "Set/unset custom message command",
		.func = _cmd_message_set,
		.admin_only = 1,
	},
	{
		.name = "/help",
		.description = "Show available commands",
		.func = _cmd_help,
	},

	/* END */
	{  NULL },
};
