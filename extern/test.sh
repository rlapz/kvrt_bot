#!/bin/bash

echo "Executable file: $0"
echo "CMD Name       : $1"
echo "Exec type      : $2"
echo "Chat ID        : $3"
echo "User ID        : $4"
echo "Message ID     : $5"
echo "Text           : $6"
echo "Raw JSON       : $7"

CFG=$TG_CONFIG_FILE

echo "exec file: $TG_API"

# Example (send plain text)
#
#     ./api [Config]        [Type]    [Data]
RES=$("$TG_API" "$CFG"  send_text "{ 'type': 'plain', 'chat_id': $3, 'message_id': $5, 'user_id': $4, 'deletable': true, 'text': 'hello'}")

MSG_ID=$(jq '.message_id' <<< "$RES")

echo $RES

[ -z "$MSG_ID" ] && exit 1

sleep 3
"$TG_API" "$CFG" delete_message "{ 'chat_id': $3, 'message_id': $MSG_ID }"


echo "$7" | jq

echo "---------------------------------------------------------------"
