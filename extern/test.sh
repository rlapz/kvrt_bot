#!/bin/sh


#
# [Bot server] -----> [Child process] ----->  [./api] -----> [Telegram server]
#
#-------------------------------------------------------------------
# Child process argument list (this file):
# CMD:
# 	0: Executable file
# 	1: CMD Name
# 	2: Exec type "cmd"
# 	3: Chat ID
# 	4: User ID
# 	5: Message ID
# 	6: Chat text
# 	7: Raw JSON
#
# Callback:
# 	0: Executable file
# 	1: CMD Name
# 	2: Exec type "callback"
# 	3: Callback ID
# 	4: Chat ID
# 	5: User ID
# 	6: Message ID
# 	7-n: User data
#
#-------------------------------------------------------------------
# "api" argument list (to send a request)
#
# Mandatory argument list:
#	[1]: CMD Name
# 	[2]: API_TYPE_*			-> see: api.h
# 	[3]: TELEGRAM API + TOKEN
# 	[4]: TELEGRAM SECRET KEY
# 	[5]: Bot ID
# 	[6]: Owner ID
#
# API_TYPE_TEXT_*:
# 	[7]: Chat ID
# 	[8]: Message ID			-> 0: no reply
# 	[9]: Text
#
# API_TYPE_LIST:
# 	[7]: Chat ID
# 	[8]: User ID
# 	[9]: Message ID
# 	[10]: Callback ID		-> "empty string: Not a callback"
# 	[11]: Context name
# 	[12]: Title
# 	[13]: Body
#
# API_TYPE_CALLBACK_ANSWER:
#	[7]: Callback ID
#	[8]: Is Text?			-> 0: URL, 1: Text
#	[9]: Text/URL
#	[10]: Show alert?		-> 0: True, 1: False
#


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
# ./api [CMD Name] [API_TYPE_PLAIN] [Telegram API + Token] [Telegram secret key] [Bot ID]  [Owner ID]  [Chat ID] [Message ID] [Text]
./api   "/test"    0                "$TG_API"              "$TG_API_SECRET_KEY"  "$BOT_ID" "$OWNER_ID" $3        $5           "hello world! from \"$1:$0\""


# print the 6-th argument: Raw JSON (see: Child process argument list)
echo "$7" | jq

echo "---------------------------------------------------------------"
