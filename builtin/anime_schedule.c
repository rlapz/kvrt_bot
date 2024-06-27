#include "anime_schedule.h"

#include "../tg_api.h"
#include "../util.h"


void
anime_schedule(TgApi *t, const TgMessage *message, const char args[])
{
	log_debug("builtin: anime_schedule");

	if (message->from != NULL)
		tg_api_send_text_plain(t, message->chat.id, message->id, "hello");

	log_debug("anime_schedule: args: %s", args);
}
