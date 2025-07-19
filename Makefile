# MIT License
#
# kvrt_bot - Telegram Bot server written in C
#
# Copyright (c) 2025 Arthur Lapz (rLapz)
#
# See LICENSE file for license details

TARGET := kvrt_bot

IS_DEBUG ?= 0
VALGRIND ?= 0
PREFIX   := /usr
CC       := cc
CFLAGS   := -std=c11 -Wall -Wextra -Wpedantic -pedantic -Wshadow -I/usr/include/json-c -D_GNU_SOURCE
LFLAGS   := -lcurl -ljson-c -lsqlite3 -lm

SRC := src/cmd.c src/common.c src/config.c src/sqlite_pool.c src/ev.c src/main.c src/model.c \
	   src/picohttpparser.c src/sched.c src/tg_api.c src/tg.c src/thrd_pool.c \
	   src/update.c src/util.c src/webhook.c \
	   src/cmd/admin.c src/cmd/general.c src/cmd/extra.c src/cmd/test.c
OBJ := $(SRC:.c=.o)

ifeq ($(IS_DEBUG), 1)
	CFLAGS := $(CFLAGS) -g -DDEBUG -O0
	LFLAGS := $(LFLAGS) -fsanitize=address -fsanitize=undefined -fsanitize=leak -Werror
else ifeq ($(VALGRIND), 1)
	CFLAGS := $(CFLAGS) -g -DDEBUG -O0
else
	CFLAGS := $(CFLAGS) -march=native -O2
endif


#------------------------------------------------------------------------------------#
build: options $(TARGET)

cmd: options $(TARGET)
	rm -f src/cmd.o

run: cmd $(TARGET)
	@if [ "$(VALGRIND)" -eq 1 ]; then				\
		valgrind --leak-check=full ./$(TARGET);		\
	else											\
		./$(TARGET);								\
	fi

$(TARGET).o:
	$(CC) $(CFLAGS) -c -o $(@) $(<)

$(TARGET): $(OBJ)
	$(CC) -o $(@) $(^) $(LFLAGS)

options:
	@echo \'$(TARGET)\' build options:
	@echo "CFLAGS = " $(CFLAGS)
	@echo "LFLAGS = " $(LFLAGS)
	@echo "CC     = " $(CC)

clean:
	rm -f $(OBJ) $(TARGET)
	rm -f config.json.bin

config:
	rm -f config.json.bin

.PHONY: build clean cmd config options run
