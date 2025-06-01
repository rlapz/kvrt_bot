#include <ctype.h>
#include <json.h>
#include <string.h>

#include "../cmd.h"
#include "../common.h"
#include "../model.h"
#include "../tg.h"
#include "../tg_api.h"


/*
 * Public
 */
void
cmd_extra_anime_sched(const Cmd *cmd)
{
	send_text_plain(cmd->msg, "TODO :3");
}