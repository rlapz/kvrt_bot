

[Bot server] -----> [Child process] ----->  [./api] -----> [Telegram server]


===========================================================================
                                CHILD PROCESS
===========================================================================
Child process argument list:
CMD:
	0: Executable file
	1: CMD Name
	2: Exec type "cmd"
	4: Chat Flags
	5: Chat ID
	6: User ID
	7: Message ID
	8: Chat text
	9: Raw JSON

Callback:
	0: Executable file
	1: CMD Name
	2: Exec type "callback"
	3: Chat Flags
	4: Chat ID
	5: User ID
	6: Message ID
	7: Callback ID
	8: Raw JSON
	9-n: User data

Standard ENV Variables:
	TG_API                  -> kvrt_bot api executable file
	TG_ROOT_DIR             -> workdir of external command
	TG_CONFIG_FILE          -> api config file (binary)
	TG_DB_MAIN_FILE         -> api database main file
	TG_DB_SCHED_FILE        -> api database scheduler file
	TG_API_URL              -> telegram api
	TG_OWNER_ID
	TG_BOT_ID
	TG_BOT_USERNAME


===========================================================================
                                   API
===========================================================================
API Argument list (to send a request):
 [0]: Config File
 [1]: CMD Name
 [2]: Api Type
 [3]: Data (JSON)

Data list:
--------------------------------------------------------------------------
 	send_text
 req:
 	{
 		"type": "plain",            -> "plain" or "format"
 		"chat_id": 00000,
 		"message_id": 111111,       -> 0: no reply
		"user_id": 222222,
 		"text": ""
 	}

 resp:
 	{
		"type": "send_text",        -> api type
		"name": "xxxx",
		"proc": "yyyy",
 		"error": "",                -> empty: success
 		"message_id": 777777777
 	}

--------------------------------------------------------------------------
 	send_photo
 req:
 	{
		"text_type": "plain",       -> "plain" or "format"
 		"chat_id": 00000,
 		"message_id": 111111,       -> 0: no reply
		"user_id": 222222,
 		"photo": "https://aaaaaa.com/photo.jpg",
		"text": ""                  -> optional, caption text
 	}

 resp:
 	{
		"type": "send_photo",       -> api type
		"name": "xxxx",
		"proc": "yyyy",
 		"error": "",                -> empty: success
 		"message_id": 777777777
 	}

--------------------------------------------------------------------------
	send_animation
 req:
	{
		"text_type": "plain",       -> "plain" or "format"
		"chat_id": 00000,
		"message_id": 111111,       -> 0: no reply
		"user_id": 222222,
		"animation": "https://aaaaaa.com/ani.gif",
		"text": ""                  -> optional, caption text
	}

 resp:
	{
		"type": "send_animation",   -> api type
		"name": "xxxx",
		"proc": "yyyy",
		"error": "",                -> empty: success
		"message_id": 777777777
	}

 --------------------------------------------------------------------------
 	answer_callback
 req:
 	{
 		"id": "xxxxxx",
 		"is_text": false,
 		"value": "xxxxxx",          -> text or URL
 		"show_alert": true
 	}

 resp:
 	{
		"type": "answer_callback",  -> api type
		"name": "xxxx",
		"proc": "yyyy",
 		"error": "",                -> empty: success
 	}

 --------------------------------------------------------------------------
 	delete_message
 req:
 	{
 		"chat_id": 00000,
 		"message_id": 000000
 	}

 resp:
 	{
		"type": "delete_message",   -> api type
		"name": "xxxx",
		"proc": "yyyy",
 		"error": "",                -> empty: success
 	}

 --------------------------------------------------------------------------
 	sched_send_message
 req:
 	{
 		"type": "send_text",        -> "send_text" or "delete"
 		"chat_id": 00000,
 		"message_id": 000000,
 		"message": "test123",       -> type: "delete" = empty
 		"timeout": 10,              -> in seconds
 		"expire": 5                 -> in seconds
 	}

 resp:
 	{
		"type": "sched_send_message", -> api type
		"name": "xxxx",
		"proc": "yyyy",
 		"error": "",                  -> empty: success
 	}

 --------------------------------------------------------------------------
 	session
 req:
 	{
 		"type": "acquire",          -> "acquire" or "release"
 		"chat_id": 00000,
 		"user_id": 000000,
 		"ctx": "abc",
 	}

 resp:
 	{
		"type": "session",            -> api type
		"name": "xxxx",
		"proc": "yyyy",
 		"error": "",                  -> empty: success
 	}


**************************************************************************
Example:
    send_text
	    [0]: "config.json.bin"
	    [1]: "send_text"
	    [2]: { "type": "plain", "chat_id": 9999999, "message_id": 0, "text": "test" }

        ./api config.json.bin send_text '{"type": "plain", "chat_id": 9999999, "message_id": 0, "text": "test" }'

        response:
			[Success]: { "type": "send_text", "name": "/xxx", "proc": "aaaa", "error": "", "message_id": xxxxxxx }
			[Error]:   { "type": "send_text", "name": "/xxx", "proc: "aaaa", "error": "Some error", "message_id": 0 }

	See: 'test.sh' file

    TODO
