#include <common.h>
#include <update.h>
#include <service.h>


int
common_privileges_check(Update *u, const TgMessage *msg)
{
	const char *resp;
	if (msg->from->id == u->owner_id)
		return 0;

	const int privs = service_admin_get_privileges(&u->service, msg->chat.id, msg->from->id);
	if (privs < 0) {
		resp = "Failed to get admin list";
		goto out0;
	}

	if (privs == 0) {
		resp = "Permission denied!";
		goto out0;
	}

	return 0;

out0:
	common_send_text_plain(u, msg, resp);
	return -1;
}


void
common_send_text_plain(Update *u, const TgMessage *msg, const char plain[])
{
	tg_api_send_text(&u->api, TG_API_TEXT_TYPE_PLAIN, msg->chat.id, &msg->id, plain);
}


void
common_send_text_format(Update *u, const TgMessage *msg, const char text[])
{
	tg_api_send_text(&u->api, TG_API_TEXT_TYPE_FORMAT, msg->chat.id, &msg->id, text);
}
