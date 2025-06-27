#include <ctype.h>
#include <json.h>
#include <string.h>

#include "../cmd.h"
#include "../common.h"
#include "../model.h"
#include "../tg.h"
#include "../tg_api.h"



typedef struct setting {
	const char *key;
	const char *description;
	void       (*callback_fn)(const struct setting *, const CmdParam *);
} Setting;


static int  _setting_list(const CmdParam *cmd);
static void _cmd_toggle_extern(const Setting *s, const CmdParam *param);
static void _cmd_toggle_extra(const Setting *s, const CmdParam *param);
static void _cmd_toggle_nsfw(const Setting *s, const CmdParam *param);
static void _cmd_toggle_flags(const CmdParam *cmd, int rflags);

static const Setting _setting_list_e[] = {
	{
		.key = "cmd_toggle_extern",
		.description = "Enable/disable Extern command",
		.callback_fn = _cmd_toggle_extern,
	},
	{
		.key = "cmd_toggle_extra",
		.description = "Enable/disable Extra command",
		.callback_fn = _cmd_toggle_extra,
	},
	{
		.key = "cmd_toggle_nsfw",
		.description = "Enable/disable NSFW command",
		.callback_fn = _cmd_toggle_nsfw,
	},
};


/*
 * Public
 */
void
cmd_admin_reload(const CmdParam *cmd)
{
	const char *resp = "Unknown error!";
	const TgMessage *const msg = cmd->msg;
	if (msg->chat.type == TG_CHAT_TYPE_PRIVATE) {
		resp = "There are no administrators in the private chat!";
		goto out0;
	}

	json_object *json;
	TgChatAdminList admin_list;
	const int64_t chat_id = msg->chat.id;
	if (tg_api_get_admin_list(chat_id, &admin_list, &json) < 0) {
		resp = "Failed to get admin list";
		goto out0;
	}

	int is_priv = 0;
	const int64_t from_id = msg->from->id;
	ModelAdmin db_admin_list[TG_CHAT_ADMIN_LIST_SIZE];
	const int db_admin_list_len = (int)admin_list.len;
	for (int i = 0; (i < db_admin_list_len) && (i < TG_CHAT_ADMIN_LIST_SIZE); i++) {
		const TgChatAdmin *const adm = &admin_list.list[i];
		if (from_id == adm->user->id) {
			if (adm->privileges == 0)
				break;

			is_priv = 1;
		}

		db_admin_list[i] = (ModelAdmin) {
			.chat_id = chat_id,
			.user_id = adm->user->id,
			.privileges = adm->privileges,
			.is_anonymous = adm->is_anonymous,
		};
	}

	if ((from_id != cmd->id_owner) && (is_priv == 0)) {
		resp = "Permission denied!";
		goto out1;
	}

	if (model_admin_reload(db_admin_list, db_admin_list_len) < 0) {
		resp = "Failed to reload admin list DB";
		goto out1;
	}

	Str str;
	char buff[1024];
	str_init(&str, buff, LEN(buff));
	resp = str_set_fmt(&str, "Done! %d admin(s) loaded", db_admin_list_len);

out1:
	json_object_put(json);
	tg_chat_admin_list_free(&admin_list);
out0:
	send_text_plain(msg, resp);
}


void
cmd_admin_cmd_message(const CmdParam *cmd)
{
	const char *resp;
	const TgMessage *const msg = cmd->msg;
	if (msg->chat.type == TG_CHAT_TYPE_PRIVATE) {
		resp = "Not supported in private chat";
		goto out0;
	}

	SpaceTokenizer st_name;
	const char *const next = space_tokenizer_next(&st_name, cmd->args);
	if (next == NULL) {
		resp = "Invalid argument!\nSet: [command_name] message ...\nUnset: [command_name] [EMPTY]";
		goto out0;
	}

	char name[MODEL_CMD_NAME_SIZE];
	if (st_name.len >= LEN(name)) {
		resp = "Command name is too long";
		goto out0;
	}

	for (unsigned i = 0; i < st_name.len; i++) {
		const int _name = st_name.value[i];
		if ((_name != '_') && (isalnum(_name) == 0)) {
			resp = "Invalid command name";
			goto out0;
		}
	}

	const char *msg_text = NULL;
	SpaceTokenizer st_args;
	if (space_tokenizer_next(&st_args, next) != NULL) {
		/* real length */
		if (strlen(st_args.value) >= MODEL_CMD_MESSAGE_VALUE_SIZE) {
			resp = "Message is too long";
			goto out0;
		}

		msg_text = st_args.value;
	}


	name[0] = '/';
	cstr_copy_n2(name + 1, LEN(name) - 1, st_name.value, st_name.len);

	int ret = model_cmd_builtin_is_exists(name);
	if (ret < 0) {
		resp = "Failed to check builtin cmd";
		goto out0;
	}

	if (ret > 0) {
		resp = "Cannot modify builtin cmd";
		goto out0;
	}

	ret = model_cmd_extern_is_exists(name);
	if (ret < 0) {
		resp = "Failed to check extern cmd";
		goto out0;
	}

	if (ret > 0) {
		resp = "Cannot modify extern cmd";
		goto out0;
	}


	const ModelCmdMessage cmd_msg = {
		.chat_id = msg->chat.id,
		.created_by = msg->from->id,
		.name_in = name,
		.value_in = msg_text,
		.updated_by = msg->from->id,
	};

	ret = model_cmd_message_set(&cmd_msg);
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
	send_text_plain(msg, resp);
}


/*
 * TODO
 */
void
cmd_admin_settings(const CmdParam *cmd)
{
	SpaceTokenizer st;
	const char *next = space_tokenizer_next(&st, cmd->args);
	if (next == NULL) {
		_setting_list(cmd);
		return;
	}

	char buff[256];
	if (st.len >= LEN(buff)) {
		send_text_plain(cmd->msg, "argument too long!");
		return;
	}

	cstr_copy_n2(buff, LEN(buff), st.value, st.len);
	for (int i = 0; i < (int)LEN(_setting_list_e); i++) {
		const Setting *const s = &_setting_list_e[i];
		if (cstr_casecmp(buff, s->key)) {
			s->callback_fn(s, cmd);
			return;
		}
	}

	send_text_plain(cmd->msg, "invalid parameter!");
}


/*
 * Private
 */
static void
_cmd_toggle_extern(const Setting *s, const CmdParam *param)
{
	_cmd_toggle_flags(param, MODEL_CHAT_FLAG_ALLOW_CMD_EXTERN);
	(void)s;
}


static void
_cmd_toggle_extra(const Setting *s, const CmdParam *param)
{
	_cmd_toggle_flags(param, MODEL_CHAT_FLAG_ALLOW_CMD_EXTRA);
	(void)s;
}


static void
_cmd_toggle_nsfw(const Setting *s, const CmdParam *param)
{
	_cmd_toggle_flags(param, MODEL_CHAT_FLAG_ALLOW_CMD_NSFW);
	(void)s;
}


static int
_setting_list(const CmdParam *cmd)
{
	Str str;
	if (str_init_alloc(&str, 1024) < 0) {
		send_text_plain(cmd->msg, "failed to allocate string buffer");
		return -1;
	}

	str_set_fmt(&str, "Available parameters:\n```parameter\n");
	for (int i = 0; i < (int)LEN(_setting_list_e); i++) {
		const Setting *const p = &_setting_list_e[i];
		str_append_fmt(&str, "%d\\. '%s' \\-> %s\n", i + 1, p->key, p->description);
	}

	str_append(&str, "```\n\\-\\-\\-\\-\nUsage: /settings \\[parameter\\] \\[ARGS\\]\n");
	str_append(&str, "Example: /settings cmd\\_toggle\\_extern\n");

	send_text_format(cmd->msg, str.cstr);
	return 0;
}


static void
_cmd_toggle_flags(const CmdParam *cmd, int rflags)
{
	const int flags = model_chat_get_flags(cmd->id_chat);
	if (flags < 0) {
		send_text_plain(cmd->msg, "failed to get chat flags!");
		return;
	}

	if (model_chat_set_flags(cmd->id_chat, (flags ^ rflags)) < 0) {
		send_text_plain(cmd->msg, "failed to set chat flags!");
		return;
	}

	if (flags & rflags)
		send_text_plain(cmd->msg, "Disabled");
	else
		send_text_plain(cmd->msg, "Enabled");
}
