#ifndef __UPDATE_H__
#define __UPDATE_H__


#include "util.h"
#include "json.h"


typedef struct {
	Str           str;
	json_value_t *json;
} Update;

int  update_init(Update *u);
void update_deinit(Update *u);
int  update_handle(Update *u, json_value_t *json);


#endif
