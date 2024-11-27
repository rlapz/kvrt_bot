#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <module.h>
#include <model.h>
#include <common.h>
#include <repo.h>


typedef void (*ModuleBuiltinFn)(Update *u, const ModuleParam *param);

typedef struct module_builti {
	int              flags;
	const char      *name;
	const char      *description;
	ModuleBuiltinFn  handler;
} ModuleBuiltin;


/*
 * Cmds
 */
static void _module_cmd_start(Update *u, const ModuleParam *param);
static void _module_cmd_admin_reload(Update *u, const ModuleParam *param);
static void _module_cmd_admin_dump(Update *u, const ModuleParam *param);
static void _module_cmd_dump(Update *u, const ModuleParam *param);
static void _module_cmd_msg_set(Update *u, const ModuleParam *param);
static void _module_cmd_msg_list(Update *u, const ModuleParam *param);
static void _module_cmd_help(Update *u, const ModuleParam *param);
static void _module_cmd_extern_list(Update *u, const ModuleParam *param);
static int  _module_cmd_msg_exec(Update *u, const TgMessage *msg, const BotCmd *cmd);


/*
 * Callbacks
 */
static void _module_cb_msg_list(Update *u, const ModuleParam *param);


/*
 * Helpers
 */
static const char *_module_hlp_msg_list_text(Str *s, const CmdMessage msgs[], int idx, int len, int max_len);


enum {
	_MODULE_START = 0,
	_MODULE_ADMIN_RELOAD,
	_MODULE_ADMIN_DUMP,
	_MODULE_DUMP,
	_MODULE_MSG_SET,
	_MODULE_MSG_LIST,
	_MODULE_HELP,
	_MODULE_EXTERN_LIST,
};

static const ModuleBuiltin _module_list[] = {
	[_MODULE_START] = {
		.name = "/start",
		.description = "Start",
		.flags = MODULE_FLAG_CMD,
		.handler = _module_cmd_start,
	},
	[_MODULE_ADMIN_RELOAD] = {
		.name = "/admin_reload",
		.description = "Reload admin list",
		.flags = MODULE_FLAG_CMD | MODULE_FLAG_ADMIN_ONLY,
		.handler = _module_cmd_admin_reload,
	},
	[_MODULE_ADMIN_DUMP] = {
		.name = "/admin_dump",
		.description = "Show admin list",
		.flags = MODULE_FLAG_CMD,
		.handler = _module_cmd_admin_dump,
	},
	[_MODULE_DUMP] = {
		.name = "/dump",
		.description = "Dump raw JSON message",
		.flags = MODULE_FLAG_CMD,
		.handler = _module_cmd_dump,
	},
	[_MODULE_MSG_SET] = {
		.name = "/msg_set",
		.description = "Set/unset command message",
		.flags = MODULE_FLAG_CMD | MODULE_FLAG_ADMIN_ONLY,
		.handler = _module_cmd_msg_set,
	},
	[_MODULE_MSG_LIST] = {
		.name = "/msg_list",
		.description = "Get command message(s)",
		.flags = MODULE_FLAG_CMD | MODULE_FLAG_CALLBACK,
		.handler = _module_cmd_msg_list,
	},
	[_MODULE_HELP] = {
		.name = "/help",
		.description = "Show builtin commands",
		.flags = MODULE_FLAG_CMD,
		.handler = _module_cmd_help,
	},
	[_MODULE_EXTERN_LIST] = {
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
module_builtin_exec_cmd(Update *update, const ModuleParam *param)
{
	const TgMessage *const msg = param->message;
	for (unsigned i = 0; i < LEN(_module_list); i++) {
		const ModuleBuiltin *const b = &_module_list[i];
		assert(b->handler != NULL);

		if (bot_cmd_compare(&param->bot_cmd, b->name) == 0)
			continue;

		if ((param->type & b->flags) == 0)
			return 1;

		if ((b->flags & MODULE_FLAG_ADMIN_ONLY) && (common_privileges_check(update, msg) < 0))
			return 1;

		b->handler(update, param);
		return 1;
	}

	return _module_cmd_msg_exec(update, param->message, &param->bot_cmd);
}


int
module_builtin_exec_callback(Update *update, const ModuleParam *param)
{
	for (unsigned i = 0; i < LEN(_module_list); i++) {
		const ModuleBuiltin *const b = &_module_list[i];
		assert(b->handler != NULL);

		if (callback_query_compare(&param->query, b->name) == 0)
			continue;

		if ((param->type & b->flags) == 0)
			return 1;

		b->handler(update, param);
		return 1;
	}

	return 0;
}


/*
 * Private
 */

/*
 * Cmds
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

	if (repo_admin_reload(&u->repo, admin_list_e, admin_list_len) < 0)
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

	char cmd_name[CMD_MSG_NAME_SIZE];
	if (args[0].len >= LEN(cmd_name)) {
		resp = "Command_name too long";
		goto out0;
	}

	for (unsigned i = 0; i < args[0].len; i++) {
		if (isalnum(args[0].value[i]) == 0) {
			resp = "Invalid command name";
			goto out0;
		}
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

		msg_text = args[1].value;
	}

	cmd_name[0] = '/';
	cstr_copy_n2(cmd_name + 1, LEN(cmd_name) - 1, args[0].value, args[0].len);

	const CmdMessage cmd_msg = {
		.chat_id = msg->chat.id,
		.created_by = msg->from->id,
		.updated_by = msg->from->id,
		.name_ptr = cmd_name,
		.message_ptr = msg_text,
	};

	const int ret = repo_cmd_message_set(&u->repo, &cmd_msg);
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
	if (param->type == MODULE_PARAM_TYPE_CALLBACK) {
		_module_cb_msg_list(u, param);
		return;
	}

	const TgMessage *const msg = param->message;
	if (msg->chat.type == TG_CHAT_TYPE_PRIVATE) {
		common_send_text_plain(u, msg, "Not supported!");
		return;
	}

	int max_len = 0;
	CmdMessage msgs[CFG_ITEM_LIST_SIZE];
	const int ret = repo_cmd_message_get_list(&u->repo, msg->chat.id, msgs, LEN(msgs), 0, &max_len);
	if (ret < 0) {
		common_send_text_plain(u, msg, "Failed to get Command Message list");
		return;
	}

	if (ret == 0) {
		common_send_text_plain(u, msg, "[Empty]");
		return;
	}

	const char *const text = _module_hlp_msg_list_text(&u->str, msgs, 1, ret, max_len);
	common_send_list(u, msg, _module_list[_MODULE_MSG_LIST].name, text, -1, max_len);
}


static void
_module_cmd_help(Update *u, const ModuleParam *param)
{
	str_set_fmt(&u->str, "Builtin commands:\n");
	for (unsigned i = 0, j = 0; i < LEN(_module_list); i++) {
		const ModuleBuiltin *const b = &_module_list[i];
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
	const int ret = repo_cmd_message_get_message(&u->repo, &cmd_msg);
	if (ret < 0) {
		common_send_text_plain(u, msg, "Failed to get command message");
		return 1;
	}

	if (ret == 0)
		return 0;

	common_send_text_plain(u, msg, cmd_msg.message);
	return 1;
}


/*
 * Callbacks
 */
static void
_module_cb_msg_list(Update *u, const ModuleParam *param)
{
	const TgMessage *const msg = param->callback->message;
	const CallbackQuery *const query = &param->query;
	if (query->args_len != 1)
		return;

	const char *const name = _module_list[_MODULE_MSG_LIST].name;
	if (cstr_cmp_n(name, query->context, query->context_len) == 0)
		return;

	const int page_num = (int)cstr_to_llong_n(query->args[0].value, query->args[0].len);
	const int req_index = (page_num - 1) * CFG_ITEM_LIST_SIZE;

	int max_len = 0;
	CmdMessage msgs[CFG_ITEM_LIST_SIZE];
	const int ret = repo_cmd_message_get_list(&u->repo, msg->chat.id, msgs, LEN(msgs),
						  req_index, &max_len);
	if (ret < 0)
		return;

	const char *const text = _module_hlp_msg_list_text(&u->str, msgs, req_index + 1, ret, max_len);

	/* TODO */
	common_send_list(u, msg, name, text, page_num, max_len);
}


/*
 * Helpers
 */
static const char *
_module_hlp_msg_list_text(Str *s, const CmdMessage msgs[], int idx, int len, int max_len)
{
	char time_buffer[DATETIME_SIZE * 2];
	char cmd_name_buffer[CMD_MSG_NAME_SIZE * 2];
	char msg_buffer[25];
	char msg_buffer_e[LEN(msg_buffer) * 2];
	str_set_fmt(s, "Command Message List: \\(%d \\- %d\\)\n", idx, max_len);

	for (int i = 0; i < len; i++) {
		const CmdMessage *const m = &msgs[i];
		const char *const date = (m->updated_at[0] == '\0')? m->created_at : m->updated_at;

		// TODO: send user_id link without mention
		//str_append_fmt(s, "%d\\. %s \\- [%" PRIi64 "](tg://user?id=%" PRIi64 ") \\- %s\n",
		//	       i + 1, common_tg_escape(cmd_name_buffer, m->name), m->created_by,
		//	       m->created_by, common_tg_escape(time_buffer, date));

		const size_t _len = cstr_copy_n(msg_buffer, LEN(msg_buffer), m->message);
		if (_len < (LEN(msg_buffer) - 4)) {
			str_append_fmt(s, "%d\\. %s \\- %s \\- _%s_\n", i + 1,
				       common_tg_escape(cmd_name_buffer, m->name),
				       common_tg_escape(msg_buffer_e, msg_buffer),
				       common_tg_escape(time_buffer, date));
		} else {
			str_append_fmt(s, "%d\\. %s \\- %s\\.\\.\\. \\- _%s_\n", i + 1,
				       common_tg_escape(cmd_name_buffer, m->name),
				       common_tg_escape(msg_buffer_e, msg_buffer),
				       common_tg_escape(time_buffer, date));
		}
	}

	return s->cstr;
}
