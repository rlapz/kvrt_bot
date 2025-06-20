

[Bot server] -----> [Child process] ----->  [./api] -----> [Telegram server]


===========================================================================
								CHILD PROCESS
===========================================================================
Child process argument list:
CMD:
	0: Executable file
	1: CMD Name
	2: Exec type "cmd"
	3: Chat ID
	4: User ID
	5: Message ID
	6: Chat text
	7: Raw JSON

Callback:
	0: Executable file
	1: CMD Name
	2: Exec type "callback"
	3: Chat ID
	4: User ID
	5: Message ID
	6: Callback ID
	7: Raw JSON
	8-n: User data

Standard ENV Variables:
	ROOT_DIR
	CONFIG_FILE
	TG_API
	CMD_PATH
	OWNER_ID
	BOT_ID
	BOT_USERNAME
	DB_PATH


===========================================================================
									API
===========================================================================
API Argument list (to send a request):
 [0]: Config File
 [1]: Api Type
 [2]: Data (JSON)

Data list:
--------------------------------------------------------------------------
 	send_text
 req:
 	{
 		"type": "plain",		    -> "plain" or "format"
 		"chat_id": 00000,
 		"message_id": 111111,		-> 0: no reply
 		"text": ""
 	}

 resp:
 	{
 		"error": "",			    -> empty: success
 		"message_id": 777777777
 	}

 --------------------------------------------------------------------------
 	answer_callback
 req:
 	{
 		"id": "xxxxxx",
 		"is_text": false,
 		"value": "xxxxxx",		    -> text or URL
 		"show_alert": true
 	}

 resp:
 	{
 		"error": "",			    -> empty: success
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
 		"error": "",			    -> empty: success
 	}

 --------------------------------------------------------------------------
 	sched_send_message
 req:
 	{
 		"type": "send_text",		-> "send_text" or "delete"
 		"chat_id": 00000,
 		"message_id": 000000,
 		"message": "test123",		-> type: "delete" = empty
 		"timeout": 10,			    -> in seconds
 		"expire": 5			        -> in seconds
 	}

 resp:
 	{
 		"error": "",			    -> empty: success
 	}


**************************************************************************
Example:
    send_text
	    [0]: "config.json"
	    [1]: "send_text"
	    [2]: { "type": "plain", "chat_id": 9999999, "message_id": 0, "text": "test" }

        ./api config.json send_text '{"type": "plain", "chat_id": 9999999, "message_id": 0, "text": "test" }'

        response:
         	[Success]: { "error": "", "message_id": xxxxxxx }
        	[Error]:   { "error": "Some error", "message_id": 0 }

    TODO
