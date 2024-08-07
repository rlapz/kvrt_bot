#include <string.h>

#include "filter.h"


void
filter_text(Module *m, const TgMessage *msg, const char text[])
{
	// TODO
	if (strstr(text, " anu ") == NULL)
		return;

	const char *const resp = str_set_fmt(&m->str, "TODO: handle 'filter'");
	if (resp == NULL)
		return;

	tg_api_send_text(m->api, TG_API_TEXT_TYPE_PLAIN, msg->chat.id, &msg->id, resp);
}
