#include <errno.h>
#include <json.h>

#include "update_manager.h"


/*
 * Public
 */
int
update_manager_init(UpdateManager *u, const char base_api[])
{
	int ret = str_init_alloc(&u->str, 1024);
	if (ret < 0) {
		log_err(ret, "update_manager: update_manager_init: str_init_alloc");
		return -1;
	}

	ret = tg_api_init(&u->api, base_api);
	if (ret < 0) {
		str_deinit(&u->str);
		return ret;
	}

	u->json = NULL;
	return 0;
}


void
update_manager_deinit(UpdateManager *u)
{
	tg_api_deinit(&u->api);
	json_object_put(u->json);
	str_deinit(&u->str);
}


int
update_manager_handle(UpdateManager *u, json_object *json)
{
	if (u->json != NULL)
		json_object_put(u->json);

	log_debug("update: %p: update_handle: \n---\n%s\n---", (void *)u,
		  json_object_to_json_string(json));

	u->json = json;
	return 0;
}


/*
 * Private
 */
