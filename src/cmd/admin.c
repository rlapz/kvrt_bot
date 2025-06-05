#include <ctype.h>
#include <json.h>
#include <string.h>

#include "../cmd.h"
#include "../common.h"
#include "../model.h"
#include "../tg.h"
#include "../tg_api.h"


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
	if (cmd_builtin_is_exists(name)) {
		resp = "Cannot modify builtin cmd";
		goto out0;
	}

	int ret = model_cmd_extern_is_exists(name);
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


void
cmd_admin_params(const CmdParam *cmd)
{
	send_text_plain(cmd->msg, "TODO");
}
