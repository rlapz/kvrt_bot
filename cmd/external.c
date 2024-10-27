#include <errno.h>

#include "external.h"

#include "../common.h"


static int _spawn_child_process(Update *u, const TgMessage *msg, json_object *json, const DbCmd *dbcmd);


/*
 * Public
 */
int
cmd_external_exec(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json)
{
	DbCmd dbcmd;
	char buffer[DB_CMD_NAME_SIZE];

	cstr_copy_n2(buffer, LEN(buffer), cmd->name, cmd->name_len);
	int ret = db_cmd_get(&u->db, &dbcmd, msg->chat.id, buffer);
	if (ret < 0)
		goto err0;

	if ((ret == 0) && (dbcmd.is_enable == 0))
		return 0;

	if ((dbcmd.is_admin_only != 0) && (common_privileges_check(u, msg) < 0))
		return 1;

	if (_spawn_child_process(u, msg, json, &dbcmd) < 0)
		goto err0;

	return 1;

err0:
	common_send_text_plain(u, msg, "Failed to execute external command");
	return 1;
}


void
cmd_external_list(Update *u)
{
	str_append_fmt(&u->str, "[TODO]\n");
}


/*
 * Private
 */
static int
_spawn_child_process(Update *u, const TgMessage *msg, json_object *json, const DbCmd *dbcmd)
{
	/* file name + NULL */
	char *argv[DB_CMD_ARGS_TYPE_SIZE + 2] = {
		[0] = (char *)dbcmd->file,
	};

	int i = 1;
	const int args = dbcmd->args;
	if (args & DB_CMD_ARG_TYPE_RAW)
		argv[i++] = (char *)json_object_to_json_string_ext(json, 0);

	char chat_id[64];
	if (args & DB_CMD_ARG_TYPE_CHAT_ID) {
		if (snprintf(chat_id, LEN(chat_id), "%"PRIi64, msg->chat.id) < 0)
			return -1;

		argv[i++] = chat_id;
	}

	char user_id[64];
	if (args & DB_CMD_ARG_TYPE_USER_ID) {
		if (snprintf(user_id, LEN(user_id), "%"PRIi64, msg->from->id) < 0)
			return -1;

		argv[i++] = user_id;
	}

	if (args & DB_CMD_ARG_TYPE_TEXT)
		argv[i] = (char *)msg->text.text;

	if (chld_spawn(u->chld, dbcmd->file, argv) < 0) {
		log_err(errno, "external: _spawn_child_process: chld_spawn: %s", dbcmd->file);
		return -1;
	}

	return 0;
}
