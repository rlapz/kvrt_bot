#include <unistd.h>

#include "../cmd.h"
#include "../common.h"
#include "../model.h"
#include "../tg_api.h"
#include "../util.h"


/*
 * Public
 */
void
cmd_test_echo(const CmdParam *cmd)
{
	send_text_plain(cmd->msg, NULL, "hello");
}


void
cmd_test_sched(const CmdParam *cmd)
{
	int64_t timeout_s = 3;
	SpaceTokenizer st_timeout;
	if (space_tokenizer_next(&st_timeout, cmd->args) != NULL)
		cstr_to_int64_n(st_timeout.value, st_timeout.len, &timeout_s);

	const TgMessage *const msg = cmd->msg;
	ModelSchedMessage sch = {
		.type = MODEL_SCHED_MESSAGE_TYPE_SEND,
		.chat_id = cmd->id_chat,
		.message_id = cmd->id_message,
		.user_id = cmd->id_user,
		.value_in = "Hello world\\!\\!\\! :3",
		.expire = 5,
	};

	if (model_sched_message_add(&sch, (int64_t)timeout_s) <= 0) {
		send_text_plain(msg, NULL, "Failed to set sechedule message");
		return;
	}

	Str str;
	char buff[256];
	str_init(&str, buff, LEN(buff));
	const char *const ss = str_set_fmt(&str, "Success! Scheduled for %lus.", timeout_s);

	int64_t ret_id;
	if (send_text_plain(msg, &ret_id, "%s", ss) < 0)
		return;

	ModelSchedMessage del = {
		.type = MODEL_SCHED_MESSAGE_TYPE_DELETE,
		.chat_id = cmd->id_chat,
		.message_id = ret_id,
		.user_id = cmd->id_user,
		.expire = 5,
	};

	model_sched_message_add(&del, 10);
}


void
cmd_test_nsfw(const CmdParam *cmd)
{
	send_text_plain(cmd->msg, NULL, "%s", "😏");
}


void
cmd_test_admin(const CmdParam *cmd)
{
	send_text_plain(cmd->msg, NULL, "Pass");
}


void
cmd_test_list(const CmdParam *cmd)
{
	send_text_plain(cmd->msg, NULL, "REGRESSED!");
}


void
cmd_test_photo(const CmdParam *cmd)
{
	const char *const photo_url = "https://cdn.nekosia.cat/images/vtuber/66aec73920d2240874bb4b11-compressed.jpg";
	send_photo_plain(cmd->msg, NULL, photo_url, "a cute girl");
}


void
cmd_test_edit(const CmdParam *cmd)
{
	int64_t msg_id = 0;
	const char *const photo_url = "https://cdn.nekosia.cat/images/vtuber/66aec73920d2240874bb4b11-compressed.jpg";

	const TgMessage msg = {
		.id = cmd->id_message,
		.from = &(TgUser) { .id = cmd->id_user},
		.chat = (TgChat) { .id = cmd->id_chat },
	};

	send_photo_plain(&msg, &msg_id, photo_url, "hello %s", "world");

	sleep(2);
	LOG_INFO("test", "%"PRIi64, cmd->id_chat);

	TgApiResp resp;
	const TgApiCaption capt = {
		.chat_id = cmd->id_chat,
		.msg_id = msg_id,
		.text = "yeah!",
		.text_type = TG_API_TEXT_TYPE_PLAIN,
		.markup = new_deleter(cmd->id_user),
	};

	tg_api_caption_edit(&capt, &resp);
	free((char *)capt.markup);
}


/*
 * Private
 */
