#include <common.h>
#include <update.h>
#include <repo.h>


int
common_privileges_check(Update *u, const TgMessage *msg)
{
	const char *resp;
	if (msg->from->id == u->owner_id)
		return 0;

	const int privs = repo_admin_get_privileges(&u->repo, msg->chat.id, msg->from->id);
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
common_send_pagination(Update *u, const TgMessage *msg, const char context[], const char body[],
		       int offt, int max)
{
	TgApiInlineKeyboard kbd = {
		.len = 0,
		.items = (TgApiInlineKeyboardItem[]) {
			{
				.text = "Next",
				.callbacks_len = 2,
				.callbacks = (TgApiCallbackData[]) {
					{ .type = TG_API_CALLBACK_DATA_TYPE_TEXT, .text = context },
					{ .type = TG_API_CALLBACK_DATA_TYPE_INT, .int_ = 2 },
				},
			},
			{
				.text = "Prev",
				.callbacks_len = 2,
				.callbacks = (TgApiCallbackData[]) {
					{ .type = TG_API_CALLBACK_DATA_TYPE_TEXT, .text = context },
					{ .type = TG_API_CALLBACK_DATA_TYPE_INT },
				},
			},
		},
	};

	/* start page */
	if (offt < 0) {
		if (CFG_ITEM_LIST_SIZE >= max) {
			common_send_text_format(u, msg, body);
			return;
		}

		kbd.len = 1;
		tg_api_send_inline_keyboard(&u->api, msg->chat.id, &msg->id, body, &kbd, 1);
		return;
	}

	TgApiInlineKeyboardItem *items = NULL;

	/* prev */
	if (offt > 1) {
		kbd.len++;
		kbd.items[1].callbacks[1].int_ = offt - 1;
		items = &kbd.items[1];
	}

	/* next */
	if ((offt * CFG_ITEM_LIST_SIZE) < max) {
		kbd.len++;
		kbd.items[0].callbacks[1].int_ = offt + 1;
		items = &kbd.items[0];
	}

	kbd.items = items;
	tg_api_edit_inline_keyboard(&u->api, msg->chat.id, msg->id, body, &kbd, 1);
}
