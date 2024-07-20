#ifndef __DB_H__
#define __DB_H__


#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <sqlite3.h>

#include "tg.h"


/* BIG TODO!! */


typedef struct db_user {
	int64_t  id;
	char    *name;
	char    *first_name;
	char    *last_name;
} DbUser;

typedef struct db_bot {
	int64_t  id;
	char    *name;
	DbUser   owner;
} DbBot;

typedef struct db_chat {
	int64_t     id;
	TgChatType  type;
	char       *name;
} DbChat;

typedef struct db_admin_rel {
	int64_t id;
	int64_t chat_id;
	int64_t user_id;
} DbAdmin;

typedef struct db_cmd {
	int32_t  id;
	char    *name;
	char    *file;
} DbCmd;

typedef struct db_cmd_arg {
	int32_t  id;
	char    *name;
} DbCmdArg;


typedef struct db_admin_role_rel {
	int32_t  id;
	int64_t  admin_id;
	uint32_t roles;
} DbAdminRoleRel;

typedef struct db_cmd_rel {
	int32_t id;
	int32_t cmd_id;
	int32_t cmd_arg_id;
} DbCmdRel;

typedef struct db_cmd_chat_rel {
	int64_t id;
	int64_t chat_id;
	int32_t cmd_rel_id;
	int32_t id_enable;
} DbCmdChatRel;


typedef struct db {
	const char      *path;
	sqlite3         *sql;
	DbBot            bot;
	pthread_mutex_t  mutex;
} Db;

int  db_init(Db *d, const char path[]);
void db_deinit(Db *d);


#endif
