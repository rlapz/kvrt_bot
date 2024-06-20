#ifndef __UPDATE_H__
#define __UPDATE_H__


#include "util.h"
#include "json.h"
#include "tg_api.h"


typedef struct {
	TgApi         api;
	json_value_t *json;
} Update;

int  update_init(Update *u, const char base_api[]);
void update_deinit(Update *u);
int  update_handle(Update *u, json_value_t *json);


#endif
