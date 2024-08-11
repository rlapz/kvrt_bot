#include <string.h>
#include <strings.h>

#include "general.h"


/*
 * public
 */
int
general_message(Module *m, const TgMessage *message, const char cmd[], unsigned cmd_len)
{
	char buffer[2048];
	cstr_copy_n2(buffer, sizeof(buffer), cmd, (size_t)cmd_len);

	if (db_cmd_message_get(m->db, buffer, sizeof(buffer), cmd) == DB_RET_OK) {
		tg_api_send_text(m->api, TG_API_TEXT_TYPE_PLAIN, message->chat.id, &message->id, buffer);
		return 1;
	}

	return 0;
}


void
general_dump(Module *m, const TgMessage *message, json_object *json_obj)
{
	const char *const json_str = json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PRETTY);
	const char *const resp = str_set_fmt(&m->str, "```json\n%s```", json_str);
	if (resp == NULL)
		return;

	tg_api_send_text(m->api, TG_API_TEXT_TYPE_FORMAT, message->chat.id, &message->id, resp);
}


void
general_dump_admin(Module *m, const TgMessage *message)
{
	json_object *json_obj;
	if (tg_api_get_admin_list(m->api, message->chat.id, NULL, &json_obj, 0) < 0)
		return;

	const char *const json_str = json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PRETTY);
	const char *const resp = str_set_fmt(&m->str, "```json\n%s```", json_str);
	if (resp == NULL) {
		json_object_put(json_obj);
		return;
	}

	tg_api_send_text(m->api, TG_API_TEXT_TYPE_FORMAT, message->chat.id, &message->id, resp);
	json_object_put(json_obj);
}


void
general_cmd_set_enable(Module *m, const TgMessage *message, const BotCmdArg args[], unsigned args_len)
{
	int is_valid = 0;
	int is_enable;
	const char *resp = NULL;
	char cmd_name[34];
	DbAdmin admin;


	if (message->from->id != m->owner_id) {
		if (db_admin_get(m->db, &admin, message->chat.id, message->from->id) == DB_RET_ERROR) {
			resp = "failed to get admin list";
			goto out0;
		}

		if (admin.is_creator == 0 && admin.privileges == 0) {
			resp = "permission denied!";
			goto out0;
		}
	}

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

	DbRet db_ret = db_cmd_set(m->db, message->chat.id, cmd_name, is_enable);
	switch (db_ret) {
	case DB_RET_ERROR:
		return;
	case DB_RET_EMPTY:
		resp = str_set_fmt(&m->str, "command: \"%s\" doesn't exists", cmd_name);
		goto out0;
	default:
		resp = str_set_fmt(&m->str, "\"%s\" now %s", cmd_name, ((is_enable) ? "enabled" : "disabled"));
		break;
	}

out1:
	if (is_valid == 0)
		resp = str_set_fmt(&m->str, "invalid argument!\n[cmd_name] [enable/disable]");

out0:
	tg_api_send_text(m->api, TG_API_TEXT_TYPE_PLAIN, message->chat.id, &message->id, resp);
}


void
general_inval(Module *m, const TgMessage *message)
{
	if (message->chat.type != TG_CHAT_TYPE_PRIVATE)
		return;

	const char *const resp = str_set_fmt(&m->str, "%s: invalid command!", message->text);
	if (resp == NULL)
		return;

	tg_api_send_text(m->api, TG_API_TEXT_TYPE_PLAIN, message->chat.id, &message->id, resp);
}


/*
 * private
 */
