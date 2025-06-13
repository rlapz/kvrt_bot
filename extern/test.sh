#!/bin/bash

echo "Executable file: $0"
echo "CMD Name       : $1"
echo "Exec type      : $2"
echo "Chat ID        : $3"
echo "User ID        : $4"
echo "Message ID     : $5"
echo "Text           : $6"
echo "Raw JSON       : $7"

# Example (send plain text)
#
# ./api [Config]        [Type]    [Data]
RES=$(./api   "$CONFIG_FILE"  send_text "{ 'type': 'plain', 'chat_id': $3, 'message_id': $5, 'text': 'hello'}")

MSG_ID=$(jq '.message_id' <<< "$RES")

[ -z "$MSG_ID" ] && echo "$RES"; exit 1

sleep 3
./api "$CONFIG_FILE" delete_message "{ 'chat_id': $3, 'message_id': $MSG_ID }"


echo "$7" | jq

echo "---------------------------------------------------------------"
