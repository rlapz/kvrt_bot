#ifndef __MODEL_H__
#define __MODEL_H__


#include <stdint.h>

#include "util.h"


/*
 * Cmd
 */
enum {
	MODEL_CMD_FLAG_TEST     = (1 << 0),
	MODEL_CMD_FLAG_CALLBACK = (1 << 1),
	MODEL_CMD_FLAG_ADMIN    = (1 << 2),
	MODEL_CMD_FLAG_NSFW     = (1 << 3),
	MODEL_CMD_FLAG_EXTRA    = (1 << 4),
};

#define MODEL_CMD_NAME_SIZE (34)
#define MODEL_CMD_DESC_SIZE (256)


/*
 * ModelChat
 */
enum {
	MODEL_CHAT_FLAG_ALLOW_CMD_NSFW   = (1 << 0),
	MODEL_CHAT_FLAG_ALLOW_CMD_EXTERN = (1 << 1),
	MODEL_CHAT_FLAG_ALLOW_CMD_EXTRA  = (1 << 2),
	MODEL_CHAT_FLAG_ALLOW_CMD_TEST   = (1 << 3),
};

typedef struct model_chat {
	int32_t id;
	int64_t chat_id;
	int     flags;
	int64_t created_at;
	int64_t created_by;
} ModelChat;

const char *model_chat_query(Str *str);
int         model_chat_get_flags(int64_t chat_id);


/*
 * ModelParam
 */
#define MODEL_PARAM_KEY_SIZE   (68)
#define MODEL_PARAM_VALUE_SIZE (256)

typedef struct model_param {
	int32_t id;
	int     is_active;
	union {
		const char *key_in;
		char        key_out[MODEL_PARAM_KEY_SIZE];
	};
	union {
		const char *value0_in;
		char        value0_out[MODEL_PARAM_VALUE_SIZE];
	};
	union {
		const char *value1_in;
		char        value1_out[MODEL_PARAM_VALUE_SIZE];
	};
	union {
		const char *value2_in;
		char        value2_out[MODEL_PARAM_VALUE_SIZE];
	};
	int64_t created_at;
	int64_t updated_at;
} ModelParam;

const char *model_param_query(Str *str);
int         model_param_get(ModelParam *p, const char key[]);


/*
 * ModelParamChat
 */
typedef struct model_param_chat {
	int32_t    id;
	int64_t    chat_id;
	ModelParam param;
} ModelParamChat;

const char *model_param_chat_query(Str *str);
int         model_param_chat_get(ModelParamChat *p, int64_t chat_id, const char key[]);



/*
 * ModelAdmin
 */
typedef struct model_admin {
	int32_t id;
	int64_t chat_id;
	int64_t user_id;
	int     is_anonymous;
	int     privileges;
	int64_t created_at;
} ModelAdmin;

const char *model_admin_query(Str *str);
int         model_admin_reload(const ModelAdmin list[], int len);
int         model_admin_get_privilegs(int64_t chat_id, int64_t user_id);


/*
 * ModelSchedMessage
 */
enum {
	MODEL_SCHED_MESSAGE_TYPE_SEND = 0,
	MODEL_SCHED_MESSAGE_TYPE_DELETE,
};

#define MODEL_SCHED_MESSAGE_VALUE_SIZE (8194)

typedef struct model_sched_message {
	int32_t id;
	int     type;
	int64_t chat_id;
	int64_t message_id;	/* 0: no message_id */
	union {
		const char *value_in;
		char        value_out[MODEL_SCHED_MESSAGE_VALUE_SIZE];
	};
	int64_t next_run;
	int64_t expire;
} ModelSchedMessage;

const char *model_sched_message_query(Str *str);
int         model_sched_message_get_list(int64_t now, ModelSchedMessage **ret_list[]);
int         model_sched_message_del_all(int64_t now);
int         model_sched_message_send(const ModelSchedMessage *s, int64_t interval_s);
int         model_sched_message_delete(const ModelSchedMessage *s, int64_t interval_s);



/*
 * ModelBlockList
 */
#define MODEL_BLOCK_LIST_REASON_SIZE (256)

typedef struct model_block_list {
	int32_t id;
	int64_t user_id;
	int64_t chat_id;	/* 0: global */
	union {
		const char *reason_in;
		char        reason_out[MODEL_BLOCK_LIST_REASON_SIZE];
	};
	int is_blocked;
} ModelBlockList;

const char *model_block_list_query(Str *str);


/*
 * ModelCmdExtern
 */
#define MODEL_CMD_EXTERN_FILENAME_SIZE (256)

typedef struct model_cmd_extern {
	int id;
	int is_enable;
	int flags;		/* MODEL_CMD_FLAG_* */
	union {
		const char *name_in;
		char        name_out[MODEL_CMD_NAME_SIZE];
	};
	union {
		const char *file_name_in;
		char        file_name_out[MODEL_CMD_EXTERN_FILENAME_SIZE];
	};
	union {
		const char *description_in;
		char        description_out[MODEL_CMD_DESC_SIZE];
	};
} ModelCmdExtern;

const char *model_cmd_extern_query(Str *str);
int         model_cmd_extern_get_one(ModelCmdExtern *c, int64_t chat_id, const char name[]);
int         model_cmd_extern_get_list(ModelCmdExtern **ret_list[], int start, int len);
int         model_cmd_extern_is_exists(const char name[]);


/*
 * ModelCmdExternDisabled
 */
typedef struct model_cmd_extern_disabled {
	int     id;
	int64_t chat_id;
	union {
		const char *name_in;
		char        name_out[MODEL_CMD_NAME_SIZE];
	};
	int64_t created_at;
} ModelCmdExternDisabled;

const char *model_cmd_extern_disabled_query(Str *str);
int         model_cmd_extern_disabled_setup(int64_t chat_id);


/*
 * ModelCmdMessage
 */
#define MODEL_CMD_MESSAGE_NAME_SIZE  MODEL_CMD_NAME_SIZE
#define MODEL_CMD_MESSAGE_VALUE_SIZE (8192)

typedef struct model_cmd_message {
	int     id;
	int64_t chat_id;
	int64_t created_by;
	int64_t created_at;
	int64_t updated_by;
	int64_t updated_at;
	union {
		const char *name_in;
		char        name_out[MODEL_CMD_MESSAGE_NAME_SIZE];
	};
	union {
		const char *value_in;
		char        value_out[MODEL_CMD_MESSAGE_VALUE_SIZE];
	};
} ModelCmdMessage;

const char *model_cmd_message_query(Str *str);
int         model_cmd_message_set(const ModelCmdMessage *c);
int         model_cmd_message_get_value(int64_t chat_id, const char name[], char value[], size_t value_len);
int         model_cmd_message_is_exists(const char name[]);


#endif
