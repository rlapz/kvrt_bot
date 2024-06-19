#!/bin/sh

export KVRT_BOT_API_TOKEN='0000000000:xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx'
export KVRT_BOT_API_SECRET='my-secret'
export KVRT_BOT_LISTEN_HOST=''
export KVRT_BOT_LISTEN_PORT=''
export KVRT_BOT_HOOK_URL='https://example.com'
export KVRT_BOT_HOOK_PATH='/bot'
export KVRT_BOT_WORKER_THREADS_NUM=4
export KVRT_BOT_WORKER_JOBS_MIN=8
export KVRT_BOT_WORKER_JOBS_MAX=32



####################################################################
if [ -z "$*" ]; then
    ./kvrt
elif [ "$*" = "set-webhook" ]; then
    echo "$*"
else
    echo "invalid argument!"
    exit 1
fi


RET_VAL="$?"
echo "return value: ${RET_VAL}"
exit $RET_VAL
####################################################################
