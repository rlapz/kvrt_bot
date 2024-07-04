#ifndef __DB_H__
#define __DB_H__


#include <stdint.h>
#include <stddef.h>
#include <sqlite3.h>


typedef struct db_user {
	int64_t  id;
	char    *username;
} DbUser;

typedef struct db_command {
	int64_t   id;
	char     *name;
	char     *executable_file;
	int32_t   __pad;
	int32_t   args_len;
	char     *args[];
} DbCommand;

typedef struct db_chat {
	int64_t    id;
	char      *type;
	char      *name;
	size_t     admin_list_len;
	DbUser    *admin_list;
	size_t     blocked_users_len;
	DbUser    *blocked_users;
	size_t     allowed_commands_len;
	DbCommand *allowed_commands;
} DbChat;

typedef struct db {
	const char *path;
	sqlite3    *sql;
} Db;

int  db_init(Db *d, const char path[]);
void db_deinit(Db *d);

int  db_chat_get_by_id(Db *d, DbChat *chat, int64_t id);
int  db_chat_get_by_name(Db *d, DbChat *chat, const char name[]);
int  db_chat_add_blocked_user(Db *d, int64_t id, int64_t user_id);
int  db_chat_del_blocked_user(Db *d, int64_t id, int64_t user_id);
int  db_chat_add_allowed_command(Db *d, int64_t id, int64_t cmd_id);
int  db_chat_del_allowed_command(Db *d, int64_t id, int64_t cmd_id);

int  db_command_add(Db *d, const char name[], const char path[], const char *args[], int32_t args_len);
int  db_command_del(Db *d, int64_t id);
int  db_command_disable(Db *d, int64_t id);
int  db_command_get_by_id(Db *d, DbCommand *cmd, int64_t id);
int  db_command_get_by_name(Db *d, DbCommand *cmd, const char name[]);


#endif
