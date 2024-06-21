#include <errno.h>
#include <json.h>

#include "update.h"


/*
 * Public
 */
int
update_init(Update *u, const char base_api[])
{
	int ret = tg_api_init(&u->api, base_api);
	if (ret < 0)
		return ret;

	u->json = NULL;
	return 0;
}


void
update_deinit(Update *u)
{
	tg_api_deinit(&u->api);
	json_object_put(u->json);
}


int
update_handle(Update *u, json_object *json)
{
	if (u->json != NULL)
		json_object_put(u->json);

	u->json = json;
	return 0;
}


/*
 * Private
 */
