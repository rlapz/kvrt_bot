#include <assert.h>
#include <string.h>

#include "builtin.h"

#include "../common.h"
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
	const char *resp;
	json_object *json_obj;
	TgChatAdminList admin_list;
	int64_t chat_id = msg->chat.id;


	if (msg->chat.type == TG_CHAT_TYPE_PRIVATE) {
		resp = "There are no administrators in the private chat!";
		goto out0;
	}

	if (tg_api_get_admin_list(&u->api, chat_id, &admin_list, &json_obj) < 0) {
		resp = "Failed to get admin list";
		goto out0;
	}

	int is_priviledged = 0;
	DbAdmin db_admins[TG_CHAT_ADMIN_LIST_SIZE];
	const int db_admins_len = (int)admin_list.len;
	for (int i = 0; (i < db_admins_len) && (i < TG_CHAT_ADMIN_LIST_SIZE); i++) {
		const TgChatAdmin *const adm = &admin_list.list[i];
		if (msg->from->id == adm->user->id) {
			if ((adm->is_creator == 0) && (adm->privileges == 0))
				goto out2;

			is_priviledged = 1;
		}

		db_admins[i] = (DbAdmin) {
			.chat_id = chat_id,
			.user_id = adm->user->id,
			.is_creator = adm->is_creator,
			.privileges = adm->privileges,
		};
	}

out2:
	if (is_priviledged == 0) {
		resp = "Permission denied!";
		goto out1;
	}

	/* TODO: use transaction */
	if (db_admin_clear(&u->db, chat_id) < 0)
		goto out1;

	if (db_admin_set(&u->db, db_admins, db_admins_len) < 0)
		goto out1;

	resp = "Done";

out1:
	json_object_put(json_obj);
	tg_chat_admin_list_free(&admin_list);
out0:
	common_send_text_plain(u, msg, resp);

	(void)cmd;
	(void)json;
}


static void
_cmd_dump(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json)
{
	const char *const json_str = json_object_to_json_string_ext(json, JSON_C_TO_STRING_PRETTY);
	common_send_text_format(u, msg, str_set_fmt(&u->str, "```json\n%s```", json_str));

	(void)cmd;
}


static void
_cmd_dump_admin(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json)
{
	if (msg->chat.type == TG_CHAT_TYPE_PRIVATE) {
		common_send_text_plain(u, msg, "There are no administrators in the private chat!");
		return;
	}

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
		resp = "Invalid argument!\nSet: [command_name] message...\nUnset: [command_name] [EMPTY]";
		goto out0;
	}

	if (args[0].len >= LEN(buffer)) {
		resp = "Command_name too long: max: 32 chars";
		goto out0;
	}

	const char *msg_text = NULL;
	if (args_len > 1) {
		unsigned max_msg_len = 0;
		for (unsigned i = 1; i < args_len -1; i++)
			max_msg_len += args[i].len;

		if (max_msg_len >= 8192) {
			resp = "Message too long: max: 8192 chars";
			goto out0;
		}

		msg_text = args[1].name;
	}

	/* TODO: validate command name */

	buffer[0] = '/';
	cstr_copy_n2(buffer + 1, LEN(buffer) - 1, args[0].name, args[0].len);
	const int ret = db_cmd_message_set(&u->db, msg->chat.id, msg->from->id, buffer, msg_text);
	if (ret < 0) {
		resp = "Failed to set command message";
		goto out0;
	}

	if (ret == 0) {
		resp = "No such command message";
		goto out0;
	}

	resp = (msg_text == NULL) ? "removed" : "ok";

out0:
	common_send_text_plain(u, msg, resp);

	(void)json;
}


static void
_cmd_help(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json)
{
	str_set_fmt(&u->str, "Builtin commands:\n");
	for (unsigned i = 0, j = 0; ; i++) {
		const CmdBuiltin *const b = &builtin_cmds[i];
		if (b->name == NULL)
			break;

		if (b->func == NULL)
			continue;

		str_append_fmt(&u->str, "%u. %s - %s %s%s\n", ++j , b->name, b->description,
			       ((b->admin_only)? "🅰️" : ""));
	}

	/* TODO: show external command list */
	str_append_fmt(&u->str, "\nExternal commands:\n");
	str_append_fmt(&u->str, "[TODO]\n");

	str_append_fmt(&u->str, "\n---\n⛔️: Disabled - 🅰️: Admin only");
	common_send_text_plain(u, msg, u->str.cstr);
	(void)cmd;
	(void)json;
}


/*
 * Public
 */
int
builtin_cmd_message(Update *u, const TgMessage *msg, const BotCmd *cmd)
{
	char buffer[DB_CMD_MSG_VAL_SIZE];
	cstr_copy_n2(buffer, LEN(buffer), cmd->name, cmd->name_len);

	const int ret = db_cmd_message_get(&u->db, buffer, LEN(buffer), msg->chat.id, buffer);
	if (ret < 0)
		return 1;

	if ((ret == 0) || (buffer[0] == '\0'))
		return 0;

	common_send_text_plain(u, msg, buffer);
	return 1;
}


const CmdBuiltin builtin_cmds[] = {
	{
		.name = "/admin_reload",
		.description = "Reload admin list",
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
		.description = "Dump admin list",
		.func = _cmd_dump_admin,
	},
	{
		.name = "/msg_set",
		.description = "Set/unset custom message command",
		.func = _cmd_message_set,
		.admin_only = 1,
	},
	{
		/* TODO */
		.name = "/cmd_list",
		.description = "Show builtin commands",
	},
	{
		/* TODO */
		.name = "/cmd_list_ext",
		.description = "Show external commands",
	},
	{
		/* TODO */
		.name = "/msg_list",
		.description = "Show custom message commands",
	},



	/* END */
	{
		.name = "/help",
		.description = "Show available commands",
		.func = _cmd_help,
	},

	{  NULL },
};
