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
	//TgApi api = {
	//	.type = TG_API_TYPE_SEND_TEXT,
	//	.parse_type = TG_API_TEXT_TYPE_FORMAT,
	//	.chat_id = cmd->id_chat,
	//	.msg_id = cmd->id_message,
	//	.value = "hello _world_",
	//};

	//int ret = tg_api_init(&api);
	//LOG_DEBUG("test", "tg_api_init: %s", tg_api_err_str(ret));

	//ret = tg_api_exec(&api);
	//LOG_DEBUG("test", "tg_api_exec: %s", tg_api_err_str(ret));

	//tg_api_deinit(&api);
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
		.chat_id = msg->chat.id,
		.message_id = ret_id,
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
	//TgApi api = {
	//	.type = TG_API_TYPE_SEND_TEXT,
	//	.parse_type = TG_API_TEXT_TYPE_FORMAT,
	//	.chat_id = cmd->id_chat,
	//	.msg_id = cmd->id_message,
	//	.value = "hello _world_",
	//};

	//TgApiKbd kbd = {
	//	.rows_len = 1,
	//	.rows = &(TgApiKbdRow) {
	//		.cols_len = 1,
	//		.cols = &(TgApiKbdButton) {
	//			.text = "hello",
	//			.url = "google.com",
	//			//.data_list_len = 1,
	//			//.data_list = &(TgApiKbdButtonData) {
	//			//	.type = TG_API_KBD_BUTTON_DATA_TYPE_TEXT,
	//			//	.text = "/help",
	//			//},
	//		},
	//	},
	//};

	//int ret = tg_api_init(&api);
	//if (ret < 0) {
	//	LOG_DEBUG("test", "tg_api_init: %s", tg_api_err_str(ret));
	//	return;
	//}

	//ret = tg_api_add_kbd(&api, &kbd);
	//if (ret < 0) {
	//	LOG_DEBUG("test", "tg_api_init: %s", tg_api_err_str(ret));
	//	goto out0;
	//}

	//ret = tg_api_exec(&api);
	//if (ret < 0) {
	//	LOG_DEBUG("test", "tg_api_exec: %s", tg_api_err_str(ret));
	//	goto out0;
	//}

	//if (ret == TG_API_ERR_API)
	//	LOG_DEBUG("test", "tg_api_exec: %s", api.api_err_msg);

	//sleep(2);
	//TgApi api2 = {
	//	.type = TG_API_TYPE_DELETE_MESSAGE,
	//	.chat_id = cmd->id_chat,
	//	.msg_id = api.ret_msg_id,
	//};

	//ret = tg_api_init(&api2);
	//if (ret < 0) {
	//	LOG_DEBUG("test", "tg_api_init: %s", tg_api_err_str(ret));
	//	goto out0;
	//}

	//ret = tg_api_exec(&api2);
	//if (ret < 0)
	//	LOG_DEBUG("test", "tg_api_exec: %s", tg_api_err_str(ret));
	//if (ret == TG_API_ERR_API)
	//	LOG_DEBUG("test", "tg_api_exec: %s", api2.api_err_msg);

	//tg_api_deinit(&api2);

//out0:
//	tg_api_deinit(&api);
}


void
cmd_test_photo(const CmdParam *cmd)
{
//	const char *const photo_url = "https://cdn.nekosia.cat/images/vtuber/66aec73920d2240874bb4b11-compressed.jpg";
//
//	TgApi api = {
//		.type = TG_API_TYPE_SEND_PHOTO,
//		.parse_type = TG_API_TEXT_TYPE_FORMAT,
//		.chat_id = cmd->id_chat,
//		.msg_id = cmd->id_message,
//		.value = photo_url,
//		.caption = "_cute girl_",
//	};
//
//	TgApiKbd kbd = {
//		.rows_len = 1,
//		.rows = &(TgApiKbdRow) {
//			.cols_len = 1,
//			.cols = &(TgApiKbdButton) {
//				.text = "hello",
//				.url = "google.com",
//				//.data_list_len = 1,
//				//.data_list = &(TgApiKbdButtonData) {
//				//	.type = TG_API_KBD_BUTTON_DATA_TYPE_TEXT,
//				//	.text = "/help",
//				//},
//			},
//		},
//	};
//
//	int ret = tg_api_init(&api);
//	if (ret < 0) {
//		LOG_DEBUG("test", "tg_api_init: %s", tg_api_err_str(ret));
//		return;
//	}
//
//	ret = tg_api_add_kbd(&api, &kbd);
//	if (ret < 0) {
//		LOG_DEBUG("test", "tg_api_init: %s", tg_api_err_str(ret));
//		goto out0;
//	}
//
//	ret = tg_api_exec(&api);
//	if (ret < 0)
//		LOG_DEBUG("test", "tg_api_exec: %s", tg_api_err_str(ret));
//
//out0:
//	tg_api_deinit(&api);
}


void
cmd_test_edit(const CmdParam *cmd)
{
//	int64_t msg_id = 0;
//	//if (SEND_TEXT_PLAIN_FMT(cmd->msg, 1, &msg_id, "%s", "hello world!") < 0)
//		//return;
//	const char *const photo_url = "https://cdn.nekosia.cat/images/vtuber/66aec73920d2240874bb4b11-compressed.jpg";
//
//	const TgMessage msg = {
//		.id = cmd->id_message,
//		.from = &(TgUser) { .id = cmd->id_user},
//		.chat = (TgChat) { .id = cmd->id_chat },
//	};
//
//	//send_photo(&msg, TG_API_PARSE_TYPE_PLAIN, photo_url, "hello world", &msg_id);
//	send_photo_fmt(&msg, TG_API_PARSE_TYPE_PLAIN, &msg_id, photo_url, "hello %s", "world");
//
//	sleep(2);
//	LOG_INFO("test", "%"PRIi64, cmd->id_chat);
//
//	//tg_api_edit_message(TG_API_EDIT_TYPE_CAPTION_PLAIN, cmd->id_chat, msg_id, "damn hell!");
//
//	TgApi api = {
//		.type = TG_API_TYPE_EDIT_CAPTION,
//		.parse_type = TG_API_TEXT_TYPE_PLAIN,
//		.chat_id = cmd->id_chat,
//		.msg_id = msg_id,
//		.caption = "damn hell!",
//	};
//
//	int ret = tg_api_init(&api);
//	if (ret < 0) {
//		LOG_DEBUG("test", "tg_api_init: %s", tg_api_err_str(ret));
//		return;
//	}
//
//	//ret = tg_api_add_kbd(&api, &kbd);
//	//if (ret < 0) {
//		//LOG_DEBUG("test", "tg_api_init: %s", tg_api_err_str(ret));
//		//goto out0;
//	//}
//
//	ret = tg_api_exec(&api);
//	if (ret < 0) {
//		LOG_DEBUG("test", "tg_api_exec: %s", tg_api_err_str(ret));
//		goto out0;
//	}
//
//	if (ret == TG_API_ERR_API)
//		LOG_DEBUG("test", "tg_api_exec: %s", api.api_err_msg);
//
//out0:
//	tg_api_deinit(&api);
}


/*
 * Private
 */
