#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include <kvrt_bot.h>


static KvrtBot *_bot;


static void
_signal_handler(int sig)
{
	printf("\nmain: _signal_handler: interrupted: %d\n", sig);
	kvrt_bot_stop(_bot);
}


static int
_set_signal_handler(KvrtBot *bot)
{
	struct sigaction act = {
		.sa_handler = SIG_IGN,
	};

	_bot = bot;
	if (sigaction(SIGPIPE, &act, NULL) < 0)
		return -1;

	/* TODO: move -> util.c: chld_init() */
	if (sigaction(SIGCHLD, &act, NULL) < 0)
		return -1;

	act.sa_handler = _signal_handler;
	if (sigaction(SIGINT, &act, NULL) < 0)
		return -1;

	return 0;
}


int
main(void)
{
	KvrtBot bot;
	if (kvrt_bot_init(&bot) < 0)
		return 1;

	int ret = _set_signal_handler(&bot);
	if (ret < 0) {
		perror("main: _set_signal_handler");
		goto out0;
	}

	ret = kvrt_bot_run(&bot);

out0:
	kvrt_bot_deinit(&bot);
	return -ret;
}
