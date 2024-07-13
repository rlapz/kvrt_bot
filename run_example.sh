#!/bin/sh


####################################################################
export KVRT_BOT_API_TOKEN='0000000000:xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx'
export KVRT_BOT_API_SECRET='my-secret'
export KVRT_BOT_LISTEN_HOST=''
export KVRT_BOT_LISTEN_PORT=''
export KVRT_BOT_HOOK_URL='example.com'
export KVRT_BOT_HOOK_PATH='/bot'
export KVRT_BOT_WORKER_THREADS_NUM=4
export KVRT_BOT_WORKER_JOBS_MIN=8
export KVRT_BOT_WORKER_JOBS_MAX=32
export KVRT_BOT_DB_FILE='./db.sql'
####################################################################


ALLOWED_UPDATES="\[\"message\",\"callback_query\",\"inline_query\"\]"

webhook_set() {
	echo "setting up webhook..."
	curl "https://api.telegram.org/bot${KVRT_BOT_API_TOKEN}/setWebhook?url=https://${KVRT_BOT_HOOK_URL}${KVRT_BOT_HOOK_PATH}&allowed_updates=${ALLOWED_UPDATES}&drop_pending_updates=True&secret_token=${KVRT_BOT_API_SECRET}"
}

webhook_del() {
	echo "deleting webhook..."
	curl "https://api.telegram.org/bot${KVRT_BOT_API_TOKEN}/deleteWebhook?drop_pending_updates=True"
}

webhook_info() {
	echo "webhook info:"
	curl "https://api.telegram.org/bot${KVRT_BOT_API_TOKEN}/getWebhookInfo"
}


if [ -z "$*" ]; then
	webhook_set
	echo

	./kvrt_bot

	webhook_del
elif [ "$*" = "webhook-set" ]; then
	webhook_set
elif [ "$*" = "webhook-del" ]; then
	webhook_del
elif [ "$*" = "webhook-info" ]; then
	webhook_info
else
	echo "invalid argument!"
	exit 1
fi


RET_VAL="$?"
printf "\nreturn value: %s\n" "${RET_VAL}"
exit $RET_VAL
