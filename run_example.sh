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
####################################################################


set_webhook() {
	echo "setting up webhook..."
	curl "https://api.telegram.org/bot${KVRT_BOT_API_TOKEN}/setWebhook?url=https://${KVRT_BOT_HOOK_URL}${KVRT_BOT_HOOK_PATH}&drop_pending_updates=true&secret_token=${KVRT_BOT_API_SECRET}"
}

del_webhook() {
	echo "deleting webhook..."
	curl "https://api.telegram.org/bot${KVRT_BOT_API_TOKEN}/setWebhook?url="
}

info_webhook() {
	echo "webhook info:"
	curl "https://api.telegram.org/bot${KVRT_BOT_API_TOKEN}/getWebhookInfo"
}


if [ -z "$*" ]; then
	./kvrt_bot
elif [ "$*" = "webhook-set" ]; then
	set_webhook
elif [ "$*" = "webhook-del" ]; then
	del_webhook
elif [ "$*" = "webhook-info" ]; then
	info_webhook
else
	echo "invalid argument!"
	exit 1
fi


RET_VAL="$?"
printf "\nreturn value: %s\n" "${RET_VAL}"
exit $RET_VAL
