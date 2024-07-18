#include <strings.h>

#include "general.h"


/*
 * public
 */
void
general_start(Module *m, const TgMessage *message, const char args[])
{
	const char *const resp = str_set_fmt(&m->str, "`TODO: handle /start`");
	if (resp == NULL)
		return;

	tg_api_send_text(m->api, TG_API_TEXT_TYPE_FORMAT, message->chat.id, &message->id, resp);

	(void)args;
}


void
general_help(Module *m, const TgMessage *message, const char args[])
{
	const char *const resp = str_set_fmt(&m->str, "`TODO: handle /help`");
	if (resp == NULL)
		return;

	tg_api_send_text(m->api, TG_API_TEXT_TYPE_FORMAT, message->chat.id, &message->id, resp);

	(void)args;
}


void
general_settings(Module *m, const TgMessage *message, const char args[])
{
	const char *const resp = str_set_fmt(&m->str, "`TODO: handle /settings`");
	if (resp == NULL)
		return;

	tg_api_send_text(m->api, TG_API_TEXT_TYPE_FORMAT, message->chat.id, &message->id, resp);

	(void)args;
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
general_test(Module *m, const TgMessage *message, const char args[])
{
	const char *ret = NULL;
	if (strcasecmp(args, "link") == 0)
		ret = "link: https://telegram.org";
	else if (strcasecmp(args, "button") == 0)
		ret = "button: [TODO]";
	else if (strcasecmp(args, "picture") == 0)
		ret = "picture: [TODO]";

	if (ret == NULL) {
		ret = "Available 'test': \n"
		       "1. Link - Send website link\n"
		       "2. Button - Send button\n"
		       "3. Picture - Send picture\n" ;
	}

	tg_api_send_text(m->api, TG_API_TEXT_TYPE_PLAIN, message->chat.id, &message->id, ret);
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
