#ifndef __ENTITY_H__
#define __ENTITY_H__


#include <stdint.h>
#include <stddef.h>

#include <tg.h>


typedef struct e_admin {
	int64_t              *chat_id;
	int64_t              *user_id;
	TgChatAdminPrivilege *privileges;
} EAdmin;


typedef struct e_module_extern {
	int     *type;
	int     *flags;
	int     *args;
	int     *args_len;
	char    *name;
	size_t   name_size;
	char    *file_name;
	size_t   file_name_size;
	char    *description;
	size_t   description_size;
} EModuleExtern;


typedef struct e_cmd_message {
	int64_t *chat_id;
	char    *name;
	size_t   name_size;
	char    *message;
	size_t   message_size;
	char    *created_at;
	size_t   created_at_size;
	int64_t *created_by;
} ECmdMessage;


#endif
