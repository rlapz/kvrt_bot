#include <string.h>

#include "anti_lewd.h"


void
anti_lewd_detect(Module *m, const TgMessage *msg, const char text[])
{
	log_debug("builtin: anti_lewd");

	if (strstr(text, " anu ") == NULL)
		return;

	const char *const resp = str_set_fmt(&m->str, "Hehe boai...");
	if (msg->from != NULL)
		tg_api_send_text_plain(m->api, msg->chat.id, msg->id, resp);
}
