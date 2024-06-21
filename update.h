#ifndef __UPDATE_H__
#define __UPDATE_H__


#include <json.h>

#include "util.h"
#include "tg_api.h"


typedef struct {
	TgApi        api;
	json_object *json;
} Update;

int  update_init(Update *u, const char base_api[]);
void update_deinit(Update *u);
int  update_handle(Update *u, json_object *json);


#endif
