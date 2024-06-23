#ifndef __UPDATE_MANAGER_H__
#define __UPDATE_MANAGER_H__


#include <json.h>

#include "util.h"
#include "tg_api.h"


typedef struct update_manager UpdateManager;
struct update_manager {
	TgApi        api;
	Str          str;
	json_object *json;
};

int  update_manager_init(UpdateManager *u, const char base_api[]);
void update_manager_deinit(UpdateManager *u);
int  update_manager_handle(UpdateManager *u, json_object *json);


#endif
