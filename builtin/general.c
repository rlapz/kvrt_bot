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
