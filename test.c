#include "util.h"
#include "db.h"


int main(void)
{
	log_init(1024);
	Db db;

	if (db_init(&db, "./db.sql") < 0) {
		log_deinit();
		return 1;
	}

	db_deinit(&db);
	log_deinit();
	return 0;
}