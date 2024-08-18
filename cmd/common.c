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


void
common_admin_reload(Update *u, const TgMessage *msg, int need_reply)
{
	const char *resp = "failed";
	json_object *json_obj;
	TgChatAdminList admin_list;
	int64_t chat_id = msg->chat.id;


	if (common_privileges_check(u, msg) < 0)
		return;

	if (tg_api_get_admin_list(&u->api, chat_id, &admin_list, &json_obj) < 0)
		goto out0;

	DbAdmin db_admins[50];
	const int db_admins_len = (int)admin_list.len;
	for (int i = 0; (i < db_admins_len) && (i < (int)LEN(db_admins)); i++) {
		const TgChatAdmin *const adm = &admin_list.list[i];
		db_admins[i] = (DbAdmin) {
			.chat_id = chat_id,
			.user_id = adm->user->id,
			.is_creator = adm->is_creator,
			.privileges = adm->privileges,
		};
	}

	if (db_admin_clear(&u->db, chat_id) < 0)
		goto out1;

	if (db_admin_set(&u->db, db_admins, db_admins_len) < 0)
		goto out1;

	resp = "done";

out1:
	json_object_put(json_obj);
	tg_chat_admin_list_free(&admin_list);
out0:
	if (need_reply)
		tg_api_send_text(&u->api, TG_API_TEXT_TYPE_PLAIN, chat_id, &msg->id, resp);
}
