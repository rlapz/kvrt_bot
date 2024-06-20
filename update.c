#include <errno.h>

#include "update.h"


/*
 * Public
 */
int
update_init(Update *u)
{
	if (str_init_alloc(&u->str, 256) < 0) {
		log_err(errno, "update: update_init: str_ini_alloc");
		return -1;
	}

	u->json = NULL;
	return 0;
}


void
update_deinit(Update *u)
{
	free(u->json);
	str_deinit(&u->str);
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