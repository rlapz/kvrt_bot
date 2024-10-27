#include "message.h"

#include "../common.h"


/*
 * Public
 */
int
cmd_message_send(Update *u, const TgMessage *msg, const BotCmd *cmd)
{
	if (msg->chat.type == TG_CHAT_TYPE_PRIVATE)
		return 0;

	char buffer[DB_CMD_MSG_VAL_SIZE];
	cstr_copy_n2(buffer, LEN(buffer), cmd->name, cmd->name_len);

	const int ret = db_cmd_message_get(&u->db, buffer, LEN(buffer), msg->chat.id, buffer);
	if (ret < 0) {
		common_send_text_plain(u, msg, "Failed to get command message");
		return 1;
	}

	if (ret == 0)
		return 0;

	common_send_text_plain(u, msg, buffer);
	return 1;
}


void
cmd_message_list(Update *u)
{
	str_append_fmt(&u->str, "[TODO]\n");
}
