#include "db.h"
#include "util.h"


static int _create_tables(sqlite3 *s);


/*
 * Public
 */
int
db_init(Db *d, const char path[])
{
	if (pthread_mutex_init(&d->mutex, NULL) != 0) {
		log_err(0, "db: db_init: pthread_mutex_init: failed");
		return -1;
	}

	sqlite3 *sql;
	if (sqlite3_open(path, &sql) != 0) {
		log_err(0, "db: db_init: sqlite3_open: %s: %s", path, sqlite3_errmsg(sql));
		goto err0;
	}

	if (_create_tables(sql) < 0)
		goto err0;

	d->sql = sql;
	d->path = path;
	return 0;

err0:
	sqlite3_close(sql);
	pthread_mutex_destroy(&d->mutex);
	return -1;
}


void
db_deinit(Db *d)
{
	sqlite3_close(d->sql);
	pthread_mutex_destroy(&d->mutex);
}


/*
 * Private
 */
static int
_create_tables(sqlite3 *s)
{
	const char user[] = "create table if not exists User("
				"id         bigint primary key not null,"	/* telegram user id */
				"name       varchar() unique not null,"		/* TODO */
				"first_name varchar() not null,"		/* TODO */
				"last_name  varchar() null"			/* TODO */
			    ");";

	const char bot[] = "create table if not exists Bot("
				"id       bigint unique not null,"		/* telegram bot id */
				"name     varchar() unique not null,"		/* TODO */
				"owner_id bigint not null"
			   ");";

	const char chat[] = "create table if not exists Chat("
				"id   bigint unique not null,"			/* telegram chat id */
				"type varchar(16) not null,"
				"name varchar() unique not null"		/* TODO */
			    ");";

	const char admin[] = "create table if not exists Admin("
				"id      integer primary key autoincrement,"
				"chat_id bigint not null,"
				"user_id bigint not null"
			     ");";

	const char cmd[] = "create table if not exists Cmd("
				"id       integer primary key autoincrement,"
				"name     varchar(128) unique not null,"
				"filename varchar(1024) not null"
			   ");";

	const char cmd_arg[] = "create table if not exists Cmd_Arg("
					"id   integer primary key autoincrement,"
					"name varchar(1024) not null"
			       ");";

	const char admin_role_rel[] = "create table if not exists Admin_Role_Rel("
					"id integer primary key autoincrement,"
					"user_id bigint not null,"
					"roles   varchar() not null"		/* TODO */
				      ");";

	const char cmd_rel[] = "create table if not exists Cmd_Rel("
					"id         integer primary key autoincrement,"
					"cmd_id     integer not null,"
					"cmd_arg_id integer not null"
			       ");";

	const char cmd_chat_rel[] = "create table if not exits Cmd_Chat_Rel("
					"id         integer primary key autoincrement,"
					"chat_id    bigint not null,"
					"cmd_rel_id integer not null,"
					"is_enable  bit"
				    ");";

	(void)s;
	(void)user;
	(void)bot;
	(void)chat;
	(void)admin;
	(void)cmd;
	(void)cmd_arg;
	(void)admin_role_rel;
	(void)cmd_rel;
	(void)cmd_chat_rel;
	return 0;
}
