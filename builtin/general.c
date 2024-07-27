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

	if (db_cmd_message_get(m->db, buffer, sizeof(buffer), cmd) == 0) {
		tg_api_send_text(m->api, TG_API_TEXT_TYPE_PLAIN, message->chat.id, &message->id, buffer);
		return 1;
	}

	return 0;
}


void
general_dump(Module *m, const TgMessage *message, json_object *json_obj)
{
	const char *const json_str = json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PRETTY);
	const char *const resp = str_set_fmt(&m->str, "```json %s```", json_str);
	if (resp == NULL)
		return;

	tg_api_send_text(m->api, TG_API_TEXT_TYPE_FORMAT, message->chat.id, &message->id, resp);
}


void
general_cmd_set(Module *m, const TgMessage *message, const BotCmd *cmd)
{
	const char *state;
	unsigned state_len;
	const char *resp = NULL;
	const BotCmdArg *const args = cmd->args;
	unsigned args_len = cmd->args_len;
	int is_enable;
	int is_valid = 0;
	int args_val = 0;
	char cmd_name[34];


	if (args_len < 2)
		goto out1;

	cmd_name[0] = '/';
	cstr_copy_n2(cmd_name + 1, LEN(cmd_name) - 1, args[0].name, args[0].len);
	if (cstr_cmp_n(cmd_name + 1, args[0].name, args[0].len) == 0) {
		resp = "invalid command name: maybe truncated";
		goto out0;
	}

	state = args[1].name;
	state_len = args[1].len;
	if (cstr_casecmp_n("enable", state, state_len)) {
		is_enable = 1;

		for (unsigned i = 0; i < args_len; i++) {
			const BotCmdArg *const _arg = &args[i];
			if (cstr_casecmp_n("chat_id", _arg->name, _arg->len))
				args_val |= DB_CMD_ARG_TYPE_CHAT_ID;
			if (cstr_casecmp_n("user_id", _arg->name, _arg->len))
				args_val |= DB_CMD_ARG_TYPE_USER_ID;
			if (cstr_casecmp_n("text", _arg->name, _arg->len))
				args_val |= DB_CMD_ARG_TYPE_TEXT;
		}
	} else if (cstr_casecmp_n("disable", state, state_len)) {
		is_enable = 0;
	} else {
		goto out1;
	}

	if (db_cmd_set(m->db, message->chat.id, cmd_name, args_val, is_enable) < 0)
		return;

	is_valid = 1;

out1:
	if (is_valid == 0) {
		resp = str_set_fmt(&m->str, "invalid argument!\n%.*s "
				   "[cmd_name] [enable/disable] [args...]",
				   (int)cmd->name_len, cmd->name);
	} else {
		resp = str_set_fmt(&m->str, "success!\n");
		resp = str_append_fmt(&m->str, "\"%s\" now accept:", cmd_name);

		if (args_val & DB_CMD_ARG_TYPE_CHAT_ID)
			resp = str_append_fmt(&m->str, " chat_id");
		if (args_val & DB_CMD_ARG_TYPE_USER_ID)
			resp = str_append_fmt(&m->str, " user_id");
		if (args_val & DB_CMD_ARG_TYPE_TEXT)
			resp = str_append_fmt(&m->str, " text");
		else
			resp = str_append_n(&m->str, " [NONE]", 7);
	}

out0:
	tg_api_send_text(m->api, TG_API_TEXT_TYPE_PLAIN, message->chat.id, &message->id, resp);
}


void
general_test(Module *m, const TgMessage *message, const BotCmdArg args[], unsigned args_len)
{
	const char *ret = NULL;
	if (args_len == 0)
		goto out0;

	const char *_arg = args[0].name;
	unsigned _arg_len = args[0].len;
	if (cstr_casecmp_n("link", _arg, _arg_len)) {
		ret = "link: https://telegram\\.org";
	} else if (cstr_casecmp_n("button", _arg, _arg_len)) {
		ret = "button: [TODO]";
	} else if (cstr_casecmp_n("photo", _arg, _arg_len)) {
		tg_api_send_photo(m->api, TG_API_PHOTO_TYPE_LINK, message->chat.id, &message->id,
				  "A cat\nsrc: https://cdn.freecodecamp.org/curriculum/cat-photo-app/relaxing-cat.jpg",
				  "https://cdn.freecodecamp.org/curriculum/cat-photo-app/relaxing-cat.jpg");
		return;
	}

	if (ret == NULL) {
out0:
		ret = "```\nAvailable 'test': \n"
		       "1. Link   - Send a website link\n"
		       "2. Button - Send buttons\n"
		       "3. Photo  - Send a photo\n"
		       "```";
	}

	tg_api_send_text(m->api, TG_API_TEXT_TYPE_FORMAT, message->chat.id, &message->id, ret);
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
