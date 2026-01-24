# kvrt_bot: A Simple Telegram Bot Server


## How to Build
### Required packages

```
        libcurl (tested on libcurl/7.81.0 and libcurl/8.15.0)
        json-c  (tested on json-c/0.18 and json-c/0.15)
        sqlite3 (tested on sqlite/3.50.2 and sqlite/3.37.2) 


        Fedora:
                dnf install libcurl-devel json-c-devel sqlite-devel
        Ubuntu:
                apt install libcurl4-openssl-dev libjson-c-dev libsqlite3-dev
```

### Building
```
        Debug mode:
                make IS_DEBUG=1 -j$(nproc)
        Release mode:
                make -j$(nproc)
```


## How to run
### Configuration
```
        1. Copy file: 'config.json.example' -> 'config.json'
        2. Edit mandatory fields:
                a. api.token            -> Bot token
                b. api.secret           -> Bot secret key (bot secret key, user defined)
                c. hook.url             -> Webhook url
                d. hook.path            -> Webhook path
                e. tg.bot_id            -> (self explanatory)
                f. tg.owner_id          -> (self explanatory)
                g. tg.bot_user_name     -> (self explanatory)
        3. run 'make config' after you modify 'config.json' file
```

### Webhook
```
        Set webhook:
                ./kvrt_bot webhook-set
        Unset webhook:
                ./kvrt_bot webhook-del
        
        *See: './kvrt_bot help'
```

### Run
```
        ./kvrt_bot
```


## Misc
### Run in local environment
```
        You can use 'ngrok' in non-production environment.
        1. Run 'ngrok http [YOUR BOT PORT]', example: 'ngrok http 8007'.
        2. Copy ngrok public addres and put it in 'config.json' file as webhook URL.
        3. Run './kvrt_bot webhook-set'
        4. Finally run './kvrt_bot'
```

### Run in production environment
```
        You must use reverse proxy, since this bot doesn't provide HTTP/SSL server.
```

### Add/set external Commands
```
        [TODO]
        see: https://github.com/rlapz/kvrt_bot_extern
```

### Configuration field explanations
```
        [TODO]
```
