#ifndef __DB_H__
#define __DB_H__


#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <sqlite3.h>


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


typedef struct db_cmd {
	char         name[128];
	char         file[1024];
	int          args_len;
	DbCmdArgType args;
} DbCmd;

typedef struct db_bot {
	int64_t id;				/* telegram bot id */
	int64_t owner_id;			/* telegram user id */
	char    name[64];
} DbBot;

typedef struct db {
	const char      *path;
	sqlite3         *sql;
	DbBot            bot;
	pthread_mutex_t  mutex;
} Db;

int  db_init(Db *d, const char path[]);
void db_deinit(Db *d);

int  db_owner_set(Db *d, int64_t bot_id, int64_t owner_id, const char name[]);

int  db_admin_set(Db *d, int64_t chat_id, int64_t user_id, DbAdminRoleType roles);
int  db_admin_ban_user(Db *d, int is_ban, int64_t chat_id, int64_t user_id);

int  db_cmd_set_enable(Db *d, int64_t chat_id, const char name[], int is_enable);
int  db_cmd_get_by_name(Db *d, DbCmd *cmd, int64_t chat_id, const char name[]);


#endif
