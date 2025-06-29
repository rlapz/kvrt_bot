#ifndef __UPDATE_H__
#define __UPDATE_H__


#include <stdint.h>
#include <json.h>


typedef struct update {
	int64_t      id_bot;
	int64_t      id_owner;
	const char  *username;
	json_object *resp;
} Update;

void update_handle(const Update *u);


#endif
