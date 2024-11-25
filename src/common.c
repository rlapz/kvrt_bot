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


char *
common_tg_escape(char dest[], const char src[])
{
	const char *const special_chars = "_*[]()~`>#+-=|{}.!";
	return cstr_escape(dest, special_chars, '\\', src);
}
