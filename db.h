#ifndef __DB_H__
#define __DB_H__


#include <stdint.h>
#include <stddef.h>
#include <sqlite3.h>

#include "util.h"


typedef enum db_admin_role_type {
	DB_ADMIN_ROLE_TYPE_CHANGE_GROUP_INFO   = (1 << 0),
	DB_ADMIN_ROLE_TYPE_DELETE_MESSAGE      = (1 << 1),
	DB_ADMIN_ROLE_TYPE_BAN_USER            = (1 << 2),
	DB_ADMIN_ROLE_TYPE_ADD_MEMBER          = (1 << 3),
	DB_ADMIN_ROLE_TYPE_PIN_MESSAGE         = (1 << 4),
	DB_ADMIN_ROLE_TYPE_MANAGE_STORY_POST   = (1 << 5),
	DB_ADMIN_ROLE_TYPE_MANAGE_STORY_EDIT   = (1 << 6),
	DB_ADMIN_ROLE_TYPE_MANAGE_STORY_DELETE = (1 << 7),
	DB_ADMIN_ROLE_TYPE_MANAGE_VIDEO_CHAT   = (1 << 8),
	DB_ADMIN_ROLE_TYPE_MANAGE_REMAIN_ANON  = (1 << 9),
} DbAdminRoleType;


typedef enum db_cmd_arg_type {
	DB_CMD_ARG_TYPE_CHAT_ID = (1 << 0),
	DB_CMD_ARG_TYPE_USER_ID = (1 << 1),
	DB_CMD_ARG_TYPE_TEXT    = (1 << 2),
} DbCmdArgType;


typedef struct db_admin {
	DbAdminRoleType roles;
	int64_t         chat_id;
	int64_t         user_id;
} DbAdmin;

typedef struct db_admin_gban_user {
	int     is_gban;
	int64_t user_id;
	int64_t chat_id;
	char    reason[2048];
} DbAdminGbanUser;

typedef struct db_cmd {
	int          is_enable;
	int64_t      chat_id;
	char         name[34];
	char         file[1024];
	int          args_len;
	DbCmdArgType args;
} DbCmd;

typedef struct db_bot {
	int64_t id;				/* telegram bot id */
	int64_t owner_id;			/* telegram user id */
} DbBot;

typedef struct db {
	const char *path;
	sqlite3    *sql;
	DbBot       bot;
} Db;

int  db_init(Db *d, const char path[]);
void db_deinit(Db *d);

int  db_admin_set(Db *d, int64_t chat_id, int64_t user_id, DbAdminRoleType roles);
int  db_admin_gban_user_set(Db *d, int64_t chat_id, int64_t user_id, int is_gban, const char reason[]);
int  db_admin_gban_user_get(Db *d, DbAdminGbanUser *gban, int64_t chat_id, int64_t user_id);

int  db_cmd_set(Db *d, int64_t chat_id, const char name[], int is_enable);
int  db_cmd_get(Db *d, DbCmd *cmd, int64_t chat_id, const char name[]);

int  db_cmd_message_get(Db *d, char buffer[], size_t size, const char name[]);


#endif
