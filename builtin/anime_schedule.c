#include "anime_schedule.h"

#include "../tg_api.h"
#include "../util.h"


void
anime_schedule(TgApi *t, const TgMessage *m)
{
	(void)m;
	log_debug("builtin: anime_schedule");

	if (m->from != NULL)
		tg_api_send_text_plain(t, m->chat.id, m->id, "hello");
}
