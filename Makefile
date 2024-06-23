# MIT License
#
# kvrt_bot - Telegram Bot written in C
#
# Copyright (c) 2024 Arthur Lapz (rLapz)
#
# See LICENSE file for license details

TARGET  = kvrt_bot
VERSION = 0.0.1

PREFIX = /usr
CC     = cc
#CFLAGS = -std=c11 -Wall -Wextra -pedantic -I/usr/include/json-c -D_POSIX_C_SOURCE=200112L -O3
CFLAGS = -g -std=c11 -Wall -Wextra -pedantic -I/usr/include/json-c -D_POSIX_C_SOURCE=200112L -DDEBUG
#LFLAGS = -lcurl -ljson-c
LFLAGS = -lcurl -ljson-c -fsanitize=address -fsanitize=undefined

SRC    = util.c config.c thrd_pool.c tg.c update_manager.c picohttpparser.c kvrt_bot.c main.c
OBJ    = $(SRC:.c=.o)
########

all: options $(TARGET)
kvrt_bot.o: $(TARGET).c
	@printf "\n%s\n" "Compiling: $(<)..."
	$(CC) $(CFLAGS) -c -o $(@) $(<)

$(TARGET): $(OBJ)
	@printf "\n%s\n" "Linking: $(^)..."
	$(CC) -o $(@) $(^) $(LFLAGS)

options:
	@echo $(TARGET) build options:
	@echo "CFLAGS" = $(CFLAGS)
	@echo "CC"     = $(CC)

clean:
	@echo cleaning...
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
