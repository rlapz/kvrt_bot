#include "general.h"


void
general_start(Module *m, const TgMessage *message)
{
	const char *const resp = str_set_fmt(&m->str, "TODO: handle /start");
	tg_api_send_text_plain(m->api, message->chat.id, message->id, resp);
}


void
general_help(Module *m, const TgMessage *message)
{
	const char *const resp = str_set_fmt(&m->str, "`TODO: handle /help`");
	tg_api_send_text_format(m->api, message->chat.id, message->id, resp);
}

void
general_dump(Module *m, const TgMessage *message, json_object *json_obj)
{
	const char *const json_str = json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PRETTY);
	tg_api_send_text_format(m->api, message->chat.id, message->id,
				str_set_fmt(&m->str, "```json %s```", json_str));
}
