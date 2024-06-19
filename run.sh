#!/bin/sh


### user-defined config, begin
export KVRT_BOT_API_TOKEN='xxx'
export KVRT_BOT_API_SECRET='bbb'
export KVRT_BOT_LISTEN_HOST=''
export KVRT_BOT_LISTEN_PORT=''
export KVRT_BOT_HOOK_URL='https://x'
export KVRT_BOT_HOOK_PATH='/bot'
export KVRT_BOT_WORKER_THREADS_NUM=8
export KVRT_BOT_WORKER_JOBS_MIN=8
export KVRT_BOT_WORKER_JOBS_MAX=32
### user-defined config, end


./kvrt


RET_VAL="$?"
echo "return value: ${RET_VAL}"
exit $RET_VAL