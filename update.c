#include <errno.h>

#include "update.h"


/*
 * Public
 */
int
update_init(Update *u, const char base_api[])
{
	if (tg_api_init(&u->api, base_api) < 0) {
		log_err(errno, "update: update_init: tg_api_init");
		return -1;
	}

	u->json = NULL;
	return 0;
}


void
update_deinit(Update *u)
{
	tg_api_deinit(&u->api);
	free(u->json);
}


int
update_handle(Update *u, json_value_t *json)
{
	if (u->json != NULL)
		free(u->json);

	u->json = json;
	return 0;
}


/*
 * Private
 */