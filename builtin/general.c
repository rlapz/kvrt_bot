#include "general.h"


static void _test_help(Module *m, const TgMessage *message);


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
	if ((args == NULL) || (args[0] == '\0')) {
		_test_help(m, message);
		return;
	}

	/* TODO */
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
static void
_test_help(Module *m, const TgMessage *message)
{
	const char *const opts =
		"Available 'test': \n"
		"1. TODO\n"
		"2. TODO\n"
		"3. TODO\n" ;

	const char *const resp = str_set_fmt(&m->str, opts);
	if (resp == NULL)
		return;

	tg_api_send_text(m->api, TG_API_TEXT_TYPE_PLAIN, message->chat.id, &message->id, resp);
}
