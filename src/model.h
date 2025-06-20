#ifndef __MODEL_H__
#define __MODEL_H__


#include <stdint.h>

#include "util.h"


int model_init(void);


/*
 * ModelChat
 */
enum {
	MODEL_CHAT_FLAG_ALLOW_CMD_NSFW   = (1 << 0),
	MODEL_CHAT_FLAG_ALLOW_CMD_EXTERN = (1 << 1),
	MODEL_CHAT_FLAG_ALLOW_CMD_EXTRA  = (1 << 2),
};

typedef struct model_chat {
	int32_t id;
	int64_t chat_id;
	int     flags;
	time_t  created_at;
} ModelChat;

int model_chat_init(int64_t chat_id);
int model_chat_set_flags(int64_t chat_id, int flags);
int model_chat_get_flags(int64_t chat_id);


/*
 * ModelAdmin
 */
typedef struct model_admin {
	int32_t id;
	int64_t chat_id;
	int64_t user_id;
	int     is_anonymous;
	int     privileges;
	time_t  created_at;
} ModelAdmin;

int model_admin_reload(const ModelAdmin list[], int len);
int model_admin_get_privilegs(int64_t chat_id, int64_t user_id);


/*
 * ModelCmd
 */
enum {
	MODEL_CMD_FLAG_EXTERN   = (1 << 0),
	MODEL_CMD_FLAG_CALLBACK = (1 << 1),
	MODEL_CMD_FLAG_ADMIN    = (1 << 2),
	MODEL_CMD_FLAG_NSFW     = (1 << 3),
	MODEL_CMD_FLAG_EXTRA    = (1 << 4),
};

#define MODEL_CMD_NAME_SIZE (32)
#define MODEL_CMD_DESC_SIZE (256)

typedef struct model_cmd {
	int  flags;
	char name[MODEL_CMD_NAME_SIZE];
	char description[MODEL_CMD_DESC_SIZE];
} ModelCmd;

int model_cmd_get_list(ModelCmd list[], int len, int offset, int *total, int chat_flags);


/*
 * ModelCmdBuiltin
 */
typedef struct model_cmd_builtin {
	int32_t id;
	int     idx;
	int     flags;
	union {
		const char *name_in;
		char        name[MODEL_CMD_NAME_SIZE];
	};
	union {
		const char *description_in;
		char        description[MODEL_CMD_DESC_SIZE];
	};
} ModelCmdBuiltin;

int model_cmd_builtin_add(const ModelCmdBuiltin *c);
int model_cmd_builtin_clear(void);
int model_cmd_builtin_get_index(const char name[], int *index);
int model_cmd_builtin_is_exists(const char name[]);


/*
 * ModelCmdExtern
 */
#define MODEL_CMD_EXTERN_FILE_NAME_SIZE (4096)

typedef struct model_cmd_extern {
	int32_t id;
	int     is_enable;
	int     flags;
	char    name[MODEL_CMD_NAME_SIZE];
	char    file_name[MODEL_CMD_EXTERN_FILE_NAME_SIZE];
	char    description[MODEL_CMD_DESC_SIZE];
} ModelCmdExtern;

int model_cmd_extern_get(ModelCmdExtern *c, const char name[]);
int model_cmd_extern_is_exists(const char name[]);


/*
 * ModelCmdMessage
 */
#define MODEL_CMD_MESSAGE_NAME_SIZE  MODEL_CMD_NAME_SIZE
#define MODEL_CMD_MESSAGE_VALUE_SIZE (8192)

typedef struct model_cmd_message {
	int32_t id;
	int64_t chat_id;
	int64_t created_by;
	time_t  created_at;
	int64_t updated_by;
	time_t  updated_at;
	union {
		const char *name_in;
		char        name[MODEL_CMD_MESSAGE_NAME_SIZE];
	};
	union {
		const char *value_in;
		char        value[MODEL_CMD_MESSAGE_VALUE_SIZE];
	};
} ModelCmdMessage;

int model_cmd_message_set(const ModelCmdMessage *c);
int model_cmd_message_get_value(int64_t chat_id, const char name[], char value[], size_t value_len);
int model_cmd_message_is_exists(const char name[]);


/*
 * ModelSchedMessage
 */
enum {
	MODEL_SCHED_MESSAGE_TYPE_SEND = 0,
	MODEL_SCHED_MESSAGE_TYPE_DELETE,

	_MODEL_SCHED_MESSAGE_TYPE_SIZE,
};

#define MODEL_SCHED_MESSAGE_VALUE_SIZE (8192)

typedef struct model_sched_message {
	int32_t id;
	int     type;
	int64_t chat_id;
	int64_t message_id;	/* 0: no message_id */
	union {
		const char *value_in;
		char        value[MODEL_SCHED_MESSAGE_VALUE_SIZE];
	};
	time_t  next_run;
	time_t  expire;
} ModelSchedMessage;

int model_sched_message_get_list(ModelSchedMessage *list[], int len, time_t now);
int model_sched_message_delete(int32_t list[], int len);
int model_sched_message_add(const ModelSchedMessage *s, time_t interval_s);


#endif
