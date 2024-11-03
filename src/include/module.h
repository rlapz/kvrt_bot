#ifndef __MODULE_H__
#define __MODULE_H__


#include <stdint.h>
#include <json.h>

#include <update.h>
#include <tg_api.h>
#include <util.h>


#define MODULE_BUILTIN_CMD_MSG_NAME_SIZE  (34)
#define MODULE_BUILTIN_CMD_MSG_VALUE_SIZE (8194)

enum {
	MODULE_TYPE_CMD = 0,
	MODULE_TYPE_CALLBACK,
};

enum {
	MODULE_FLAG_ADMIN_ONLY = (1 << 0),
	MODULE_FLAG_NSFW       = (1 << 1),
};


/*
 * Builtin
 */
int module_builtin_exec(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json);


/*
 * Extern
 */
#define MODULE_EXTERN_NAME_SIZE      (34)
#define MODULE_EXTERN_FILE_NAME_SIZE (1024)
#define MODULE_EXTERN_DESC_SIZE      (256)
#define MODULE_EXTERN_ARGS_SIZE      (4)

enum {
	MODULE_EXTERN_ARG_RAW     = (1 << 0),
	MODULE_EXTERN_ARG_CHAT_ID = (1 << 1),
	MODULE_EXTERN_ARG_USER_ID = (1 << 2),
	MODULE_EXTERN_ARG_TEXT    = (1 << 3),
};

typedef struct module_extern {
	int     type;
	int     flags;
	int     args;
	int     args_len;
	int64_t chat_id;
	char    name[MODULE_EXTERN_NAME_SIZE];
	char    file_name[MODULE_EXTERN_FILE_NAME_SIZE];
	char    description[MODULE_EXTERN_DESC_SIZE];
} ModuleExtern;

int  module_extern_exec(Update *update, const TgMessage *msg, const BotCmd *cmd, json_object *json);


#endif
