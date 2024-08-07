#ifndef __DB_H__
#define __DB_H__


#include <stdint.h>
#include <stddef.h>
#include <sqlite3.h>

#include "tg.h"
#include "util.h"


typedef enum db_cmd_arg_type {
	DB_CMD_ARG_TYPE_CHAT_ID = (1 << 0),
	DB_CMD_ARG_TYPE_USER_ID = (1 << 1),
	DB_CMD_ARG_TYPE_TEXT    = (1 << 2),
} DbCmdArgType;


typedef struct db_admin {
	int                  is_creator;
	int64_t              chat_id;
	int64_t              user_id;
	TgChatAdminPrivilege privileges;
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

int  db_admin_set(Db *d, int64_t chat_id, int64_t user_id, int is_creator, TgChatAdminPrivilege privileges);
int  db_admin_get(Db *d, DbAdmin *admin, int64_t chat_id, int64_t user_id);
int  db_admin_clear(Db *d, int64_t chat_id);
int  db_admin_gban_user_set(Db *d, int64_t chat_id, int64_t user_id, int is_gban, const char reason[]);
int  db_admin_gban_user_get(Db *d, DbAdminGbanUser *gban, int64_t chat_id, int64_t user_id);

int  db_cmd_set(Db *d, int64_t chat_id, const char name[], int is_enable);
int  db_cmd_get(Db *d, DbCmd *cmd, int64_t chat_id, const char name[]);

int  db_cmd_message_get(Db *d, char buffer[], size_t size, const char name[]);


#endif
