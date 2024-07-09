#include "general.h"


void
general_start(Module *m, const TgMessage *message, const char args[])
{
	(void)args;
	log_debug("builtin: general_start");

	const char *const resp = str_set_fmt(&m->str, "Hello! How can I help you? :)");
	if (message->from != NULL)
		tg_api_send_text_plain(m->api, message->chat.id, message->id, resp);
}


void
general_help(Module *m, const TgMessage *message, const char args[])
{
	(void)args;
	log_debug("builtin: general_help");

	const char *const resp = str_set_fmt(&m->str, "`I` can't *help* `you`\\! :\\(");
	if (message->from != NULL)
		tg_api_send_text_format(m->api, message->chat.id, message->id, resp);
}

void
general_dump(Module *m, const TgMessage *message, json_object *json_obj)
{
	if (message->from == NULL)
		return;

	const char *const json_str = json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PRETTY);
	const char *const json_fmt = str_set_fmt(&m->str, "```json %s```", json_str);

	tg_api_send_text_format(m->api, message->chat.id, message->id, json_fmt);
}
