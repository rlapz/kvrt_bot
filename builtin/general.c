#include <string.h>
#include <strings.h>

#include "general.h"


/*
 * public
 */
void
general_start(Module *m, const TgMessage *message, const BotCmdArg args[], unsigned args_len)
{
	const char *const resp = str_set_fmt(&m->str, "`TODO: handle /start`");
	if (resp == NULL)
		return;

	tg_api_send_text(m->api, TG_API_TEXT_TYPE_FORMAT, message->chat.id, &message->id, resp);

	(void)args;
	(void)args_len;
}


void
general_help(Module *m, const TgMessage *message, const BotCmdArg args[], unsigned args_len)
{
	const char *const resp = str_set_fmt(&m->str, "`TODO: handle /help`");
	if (resp == NULL)
		return;

	tg_api_send_text(m->api, TG_API_TEXT_TYPE_FORMAT, message->chat.id, &message->id, resp);

	(void)args;
	(void)args_len;
}


void
general_settings(Module *m, const TgMessage *message, const BotCmdArg args[], unsigned args_len)
{
	const char *const resp = str_set_fmt(&m->str, "`TODO: handle /settings`");
	if (resp == NULL)
		return;

	tg_api_send_text(m->api, TG_API_TEXT_TYPE_FORMAT, message->chat.id, &message->id, resp);

	(void)args;
	(void)args_len;
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
general_cmd_set(Module *m, const TgMessage *message, const BotCmdArg args[], unsigned args_len)
{
	const char *_arg = "";
	unsigned _arg_len = 0;
	const char *resp = NULL;
	int is_enable = -1;


	if (args_len > 0) {
		_arg = args[0].name;
		_arg_len = args[0].len;

		if (cstr_casecmp_n("enable", _arg, _arg_len))
			is_enable = 1;
		else if (cstr_casecmp_n("disable", _arg, _arg_len))
			is_enable = 0;
	} else {
		_arg = "[empty]";
		_arg_len = strlen(_arg);
	}

	if (is_enable < 0) {
		resp = str_set_fmt(&m->str, "%.*s: invalid argument!", (int)_arg_len, _arg);
		goto out0;
	}

	// TODO
	resp = "ok";

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
