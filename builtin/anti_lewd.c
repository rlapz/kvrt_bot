#include <string.h>

#include "anti_lewd.h"


void
anti_lewd_detect_text(Module *m, const TgMessage *msg, const char text[])
{
	// TODO
	if (strstr(text, " anu ") == NULL)
		return;

	const char *const resp = str_set_fmt(&m->str, "TODO: handle 'lewd' text");
	tg_api_send_text_plain(m->api, msg->chat.id, msg->id, resp);
}
