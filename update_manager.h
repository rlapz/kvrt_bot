#ifndef __UPDATE_MANAGER_H__
#define __UPDATE_MANAGER_H__


#include <json.h>

#include "util.h"
#include "module.h"
#include "tg_api.h"


typedef struct update_manager {
	TgApi  api;
	Module module;
} UpdateManager;

int  update_manager_init(UpdateManager *u, const char base_api[]);
void update_manager_deinit(UpdateManager *u);
void update_manager_handle(UpdateManager *u, json_object *json_obj);


#endif
