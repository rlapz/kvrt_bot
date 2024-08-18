#include "common.h"

#include "../update.h"


int
common_privileges_check(Update *u, const TgMessage *msg)
{
	if (msg->from->id == u->owner_id)
		return 0;

	DbAdmin admin;
	const char *resp;
	const int ret = db_admin_get(&u->db, &admin, msg->chat.id, msg->from->id);
	if (ret < 0) {
		resp = "failed to get admin list";
		goto out0;
	}

	if ((ret == 0) || ((admin.is_creator == 0) && (admin.privileges == 0))) {
		resp = "permission denied!";
		goto out0;
	}

	return 0;

out0:
	tg_api_send_text(&u->api, TG_API_TEXT_TYPE_PLAIN, msg->chat.id, &msg->id, resp);
	return -1;
}


int
common_cmd_message(Update *u, const TgMessage *msg, const BotCmd *cmd)
{
	char buffer[8194];
	cstr_copy_n2(buffer, LEN(buffer), cmd->name, cmd->name_len);

	const int ret = db_cmd_message_get(&u->db, buffer, LEN(buffer), msg->chat.id, buffer);
	if (ret < 0)
		return 1;

	if (ret == 0)
		return 0;

	if (buffer[0] != '\0')
		tg_api_send_text(&u->api, TG_API_TEXT_TYPE_PLAIN, msg->chat.id, &msg->id, buffer);

	return 1;
}


void
common_cmd_invalid(Update *u, const TgMessage *msg)
{
	if (msg->chat.type != TG_CHAT_TYPE_PRIVATE)
		return;

	const char *const resp = str_set_fmt(&u->str, "%s: invalid command!", msg->text);
	if (resp == NULL)
		return;

	tg_api_send_text(&u->api, TG_API_TEXT_TYPE_PLAIN, msg->chat.id, &msg->id, resp);
}
