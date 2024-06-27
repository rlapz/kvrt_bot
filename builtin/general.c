#include "general.h"


void
general_start(TgApi *t, const TgMessage *message, const char args[])
{
	(void)args;
	log_debug("builtin: general_start");

	if (message->from != NULL)
		tg_api_send_text_plain(t, message->chat.id, message->id, "hello :)");
}


void
general_help(TgApi *t, const TgMessage *message, const char args[])
{
	(void)args;
	log_debug("builtin: general_help");

	if (message->from != NULL)
		tg_api_send_text_plain(t, message->chat.id, message->id, "I can't help you! :(");
}
