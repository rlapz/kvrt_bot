#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "util.h"


int main(void)
{
	log_init(4096);

	Chld chld;
	if (chld_init(&chld) < 0)
		goto out0;

	char *argv[] = { "chld", NULL };
	chld_spawn(&chld, "/tmp/chld", argv);
	chld_spawn(&chld, "/tmp/chld", argv);
	chld_spawn(&chld, "/tmp/chld", argv);


	for (int i = 0; i < 13; i++) {
		chld_reap(&chld);
		sleep(1);

		if (i > 10)
			chld_spawn(&chld, "/tmp/chld", argv);
	}

	puts("deinit");
	chld_deinit(&chld);
	puts("end");
	sleep(3);

out0:
	log_deinit();
	return 0;
}