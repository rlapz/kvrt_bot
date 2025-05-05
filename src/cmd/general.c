#include <assert.h>
#include <math.h>

#include "../cmd.h"
#include "../common.h"
#include "../model.h"
#include "../util.h"


static const char *const _icon_admin  = "🅰️";
static const char *const _icon_nsfw   = "🔞";
static const char *const _icon_extern = "🗿";


/*
 * Public
 */
void
cmd_general_start(const Cmd *cmd)
{
	ModelParam param;
	if (model_param_get(&param, "msg_start") <= 0) {
		send_text_format(cmd->msg, "Hello \\:3");
		return;
	}

	send_text_format(cmd->msg, param.value0_out);
}


void
cmd_general_help(const Cmd *cmd)
{
	(void)_icon_admin;
	(void)_icon_nsfw;
	(void)_icon_extern;
	(void)cmd;
}


void
cmd_general_dump(const Cmd *cmd)
{
	Str str;
	if (str_init_alloc(&str, 0) < 0)
		return;

	const char *const json_str = json_object_to_json_string_ext(cmd->json, JSON_C_TO_STRING_PRETTY);
	send_text_format(cmd->msg, str_set_fmt(&str, "```json\n%s```", json_str));

	str_deinit(&str);
}


void
cmd_general_dump_admin(const Cmd *cmd)
{
	const TgMessage *msg = cmd->msg;
	if (msg->chat.type == TG_CHAT_TYPE_PRIVATE) {
		send_text_plain(msg, "There are no administrators in the private chat!");
		return;
	}

	json_object *json;
	if (tg_api_get_admin_list(msg->chat.id, NULL, &json) < 0) {
		send_text_plain(msg, "Failed to get admin list");
		return;
	}

	Cmd _cmd = {
		.msg = msg,
		.json = json,
	};

	cmd_general_dump(&_cmd);
	json_object_put(json);
}


/*
 * Private
 */
