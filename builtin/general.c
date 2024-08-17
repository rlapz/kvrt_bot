#include <string.h>
#include <strings.h>

#include "general.h"
#include "../util.h"


/*
 * public
 */
void
general_message_set(Module *m, const TgMessage *message, const BotCmdArg args[], int args_len)
{
	const char *resp;
	char buffer[34];
	if ((message->chat.type != TG_CHAT_TYPE_PRIVATE) && general_admin_check(m, message) < 0)
		return;

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
		for (int i = 1; i < args_len -1; i++)
			max_msg_len += args[i].len;

		if (max_msg_len >= 8192) {
			resp = "message too long: max: 8192 chars";
			goto out0;
		}

		msg_text = args[1].name;
	}

	buffer[0] = '/';
	cstr_copy_n2(buffer + 1, LEN(buffer) - 1, args[0].name, args[0].len);
	const int ret = db_cmd_message_set(&m->db, message->chat.id, message->from->id, buffer, msg_text);
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
	tg_api_send_text(&m->api, TG_API_TEXT_TYPE_PLAIN, message->chat.id, &message->id, resp);
}


int
general_message_get(Module *m, const TgMessage *message, const char cmd[], unsigned cmd_len)
{
	char buffer[8193];
	cstr_copy_n2(buffer, LEN(buffer), cmd, (size_t)cmd_len);

	const int ret = db_cmd_message_get(&m->db, buffer, sizeof(buffer), message->chat.id, cmd);
	if (ret < 0)
		return 1;

	if (ret == 0)
		return 0;

	if (buffer[0] != '\0')
		tg_api_send_text(&m->api, TG_API_TEXT_TYPE_PLAIN, message->chat.id, &message->id, buffer);

	return 1;
}


void
general_dump(Module *m, const TgMessage *message, json_object *json_obj)
{
	const char *const json_str = json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PRETTY);
	const char *const resp = str_set_fmt(&m->str, "```json\n%s```", json_str);
	if (resp == NULL) {
		tg_api_send_text(&m->api, TG_API_TEXT_TYPE_PLAIN, message->chat.id, &message->id, "failed");
		return;
	}

	tg_api_send_text(&m->api, TG_API_TEXT_TYPE_FORMAT, message->chat.id, &message->id, resp);
}


void
general_dump_admin(Module *m, const TgMessage *message)
{
	json_object *json_obj;
	if (tg_api_get_admin_list(&m->api, message->chat.id, NULL, &json_obj) < 0)
		return;

	general_dump(m, message, json_obj);
	json_object_put(json_obj);
}


void
general_admin_reload(Module *m, const TgMessage *message)
{
	const char *resp = "failed";
	json_object *json_obj;
	TgChatAdminList admin_list;
	int64_t chat_id = message->chat.id;


	if (general_admin_check(m, message) < 0)
		return;

	if (tg_api_get_admin_list(&m->api, chat_id, &admin_list, &json_obj) < 0)
		goto out0;

	DbAdmin db_admins[50];
	const int db_admins_len = (int)admin_list.len;
	for (int i = 0; (i < db_admins_len) && (i < (int)LEN(db_admins)); i++) {
		const TgChatAdmin *const adm = &admin_list.list[i];
		db_admins[i] = (DbAdmin) {
			.chat_id = chat_id,
			.user_id = adm->user->id,
			.is_creator = adm->is_creator,
			.privileges = adm->privileges,
		};
	}

	if (db_admin_clear(&m->db, chat_id) < 0)
		goto out1;

	if (db_admin_set(&m->db, db_admins, db_admins_len) < 0)
		goto out1;

	resp = "done";

out1:
	json_object_put(json_obj);
	tg_chat_admin_list_free(&admin_list);
out0:
	tg_api_send_text(&m->api, TG_API_TEXT_TYPE_PLAIN, chat_id, &message->id, resp);
}


void
general_cmd_set_enable(Module *m, const TgMessage *message, const BotCmdArg args[], unsigned args_len)
{
	int is_valid = 0;
	int is_enable;
	const char *resp = NULL;
	char cmd_name[34];


	if (general_admin_check(m, message) < 0)
		return;

	if (args_len != 2)
		goto out1;

	cmd_name[0] = '/';
	cstr_copy_n2(cmd_name + 1, LEN(cmd_name) - 1, args[0].name, (size_t)args[0].len);
	if (cstr_cmp_n(cmd_name + 1, args[0].name, args[0].len) == 0) {
		resp = "invalid command name: maybe truncated";
		goto out0;
	}


	const char *const st = args[1].name;
	const size_t st_len = (size_t)args[1].len;
	if (cstr_casecmp_n("enable", st, st_len))
		is_enable = 1;
	else if (cstr_casecmp_n("disable", st, st_len))
		is_enable = 0;
	else
		goto out1;

	is_valid = 1;

	const int ret = db_cmd_set(&m->db, message->chat.id, cmd_name, is_enable);
	if (ret < 0)
		return;

	if (ret == 0) {
		resp = str_set_fmt(&m->str, "command: \"%s\" doesn't exists", cmd_name);
		goto out0;
	}

	resp = str_set_fmt(&m->str, "\"%s\" now %s", cmd_name, ((is_enable) ? "enabled" : "disabled"));

out1:
	if (is_valid == 0)
		resp = str_set_fmt(&m->str, "invalid argument!\n[cmd_name] [enable/disable]");

out0:
	tg_api_send_text(&m->api, TG_API_TEXT_TYPE_PLAIN, message->chat.id, &message->id, resp);
}


void
general_get_me(Module *m, const TgMessage *message)
{
	json_object *json;
	if (tg_api_get_me(&m->api, NULL, &json) < 0) {
		tg_api_send_text(&m->api, TG_API_TEXT_TYPE_PLAIN, message->chat.id, &message->id, "failed");
		return;
	}

	general_dump(m, message, json);
	json_object_put(json);
}


int
general_admin_check(Module *m, const TgMessage *message)
{
	if (message->from->id == m->owner_id)
		return 0;

	DbAdmin admin;
	const char *resp = NULL;
	const int ret = db_admin_get(&m->db, &admin, message->chat.id, message->from->id);
	if (ret < 0) {
		resp = "failed to get admin list";
		goto out0;
	}

	if ((ret == 0) || ((admin.is_creator == 0) && (admin.privileges == 0))) {
		resp = "permission denied!";
		goto out0;
	}

	return 0;

out0:
	tg_api_send_text(&m->api, TG_API_TEXT_TYPE_PLAIN, message->chat.id, &message->id, resp);
	return -1;
}


void
general_inval(Module *m, const TgMessage *message)
{
	if (message->chat.type != TG_CHAT_TYPE_PRIVATE)
		return;

	const char *const resp = str_set_fmt(&m->str, "%s: invalid command!", message->text);
	if (resp == NULL)
		return;

	tg_api_send_text(&m->api, TG_API_TEXT_TYPE_PLAIN, message->chat.id, &message->id, resp);
}


/*
 * private
 */
