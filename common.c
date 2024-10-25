#include "common.h"
#include "update.h"


int
common_privileges_check(Update *u, const TgMessage *msg)
{
	if (msg->from->id == u->owner_id)
		return 0;

	DbAdmin admin;
	const char *resp;
	const int ret = db_admin_get(&u->db, &admin, msg->chat.id, msg->from->id);
	if (ret < 0) {
		resp = "Failed to get admin list";
		goto out0;
	}

	if ((ret == 0) || ((admin.is_creator == 0) && (admin.privileges == 0))) {
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
