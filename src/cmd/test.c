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
	//send_text_plain(cmd->msg, "Ok");
	SEND_TEXT_PLAIN_FMT(cmd->msg, 1, NULL, "%s", "test");
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
		.chat_id = msg->chat.id,
		.message_id = msg->id,
		.value_in = "Hello world\\!\\!\\! :3",
		.expire = 5,
	};

	if (model_sched_message_add(&sch, (int64_t)timeout_s) <= 0) {
		SEND_TEXT_PLAIN(msg, "Failed to set sechedule message");
		return;
	}

	Str str;
	char buff[256];
	str_init(&str, buff, LEN(buff));
	const char *const ss = str_set_fmt(&str, "Success! Scheduled for %lus.", timeout_s);

	int64_t ret_id;
	if (tg_api_send_text(TG_API_TEXT_TYPE_PLAIN, msg->chat.id, msg->id, ss, &ret_id) < 0)
		return;

	ModelSchedMessage del = {
		.type = MODEL_SCHED_MESSAGE_TYPE_DELETE,
		.chat_id = msg->chat.id,
		.message_id = ret_id,
		.expire = 5,
	};

	model_sched_message_add(&del, 10);
}


void
cmd_test_nsfw(const CmdParam *cmd)
{
	SEND_TEXT_PLAIN(cmd->msg, "😏");
}


void
cmd_test_admin(const CmdParam *cmd)
{
	SEND_TEXT_PLAIN(cmd->msg, "Pass");
}


void
cmd_test_list(const CmdParam *cmd)
{
	MessageList list = {
		.id_user = cmd->id_user,
		.id_owner = cmd->id_owner,
		.id_chat = cmd->id_chat,
		.id_message = cmd->msg->id,
		.id_callback = cmd->id_callback,
		.body = "test",
		.ctx = cmd->name,
		.title = "Test List",
		.udata = "",
	};

	if (message_list_init(&list, cmd->args) < 0)
		return;

	const MessageListPagination pag = { .page_num = 1, .page_size = 1 };
	message_list_send(&list, &pag, NULL);
}


/*
 * Private
 */
