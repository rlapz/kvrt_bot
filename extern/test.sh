#!/bin/sh


echo "test" 0 "$TG_API" "$TG_API_SECRET_KEY" "$BOT_ID" "$OWNER_ID" "$@"
./api "test" 0 "$TG_API" "$TG_API_SECRET_KEY" "$BOT_ID" "$OWNER_ID" $2 $4 "hello world! from \"$0\""

echo "$6" | jq

