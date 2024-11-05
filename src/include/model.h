#ifndef __MODEL_H__
#define __MODEL_H__

#include <stdint.h>
#include <stddef.h>

#include <tg.h>


/*
 * Admin
 */
typedef struct admin {
	int64_t              chat_id;
	int64_t              user_id;
	TgChatAdminPrivilege privileges;
} Admin;


/*
 * CmdMessage
 */
#define CMD_MSG_NAME_SIZE       (34)
#define CMD_MSG_VALUE_SIZE      (8194)
#define CMD_MSG_CREATED_AT_SIZE (20)

typedef struct cmd_message {
	int64_t chat_id;
	int64_t created_by;
	char    created_at[CMD_MSG_CREATED_AT_SIZE];

	union {
		const char *name_ptr;
		char        name[CMD_MSG_NAME_SIZE];
	};

	union {
		const char *message_ptr;
		char        message[CMD_MSG_VALUE_SIZE];
	};
} CmdMessage;


/*
 * Module
 */
#define MODULE_FLAG_TYPE_CMD      (1 << 0)
#define MODULE_FLAG_TYPE_CALLBACK (1 << 1)
#define MODULE_FLAG_ADMIN_ONLY    (1 << 2)
#define MODULE_FLAG_NSFW          (1 << 3)

#define MODULE_EXTERN_ARG_RAW     (1 << 0)
#define MODULE_EXTERN_ARG_CHAT_ID (1 << 1)
#define MODULE_EXTERN_ARG_USER_ID (1 << 2)
#define MODULE_EXTERN_ARG_TEXT    (1 << 3)

#define MODULE_EXTERN_NAME_SIZE      (34)
#define MODULE_EXTERN_FILE_NAME_SIZE (1024)
#define MODULE_EXTERN_DESC_SIZE      (256)
#define MODULE_EXTERN_ARGS_SIZE      (4)


typedef struct module_extern {
	int     flags;
	int     args;
	int     args_len;
	int64_t chat_id;

	union {
		const char *name_ptr;
		char        name[MODULE_EXTERN_NAME_SIZE];
	};

	union {
		const char *file_name_ptr;
		char        file_name[MODULE_EXTERN_FILE_NAME_SIZE];
	};

	union {
		const char *description_ptr;
		char        description[MODULE_EXTERN_DESC_SIZE];
	};
} ModuleExtern;


#endif
