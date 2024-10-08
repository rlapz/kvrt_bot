# MIT License
#
# kvrt_bot - Telegram Bot written in C
#
# Copyright (c) 2024 Arthur Lapz (rLapz)
#
# See LICENSE file for license details

TARGET  := kvrt_bot
VERSION := 0.0.1

IS_DEBUG ?= 0
PREFIX   := /usr
CC       := cc
CFLAGS   := -std=c11 -Wall -Wextra -pedantic -I/usr/include/json-c -D_POSIX_C_SOURCE=200112L
LFLAGS   := -lcurl -ljson-c -lsqlite3
SRC      := main.c kvrt_bot.c util.c config.c thrd_pool.c tg.c tg_api.c \
			picohttpparser.c db.c update.c \
			cmd/common.c cmd/builtin.c
OBJ      := $(SRC:.c=.o)


ifeq ($(IS_DEBUG), 1)
	CFLAGS := $(CFLAGS) -g -DDEBUG -O0
	LFLAGS := $(LFLAGS) -fsanitize=address -fsanitize=undefined
else
	CFLAGS := $(CFLAGS) -O2
endif


build: options $(TARGET) run.sh

options:
	@echo \'$(TARGET)\' build options:
	@echo "CFLAGS =" $(CFLAGS)
	@echo "CC     =" $(CC)

$(TARGET).o: $(TARGET).c
	@printf "\n%s\n--------------------\n" "Compiling..."
	$(CC) $(CFLAGS) -c -o $(@) $(<)

$(TARGET): $(OBJ)
	@printf "\n%s\n--------------------\n" "Linking..."
	$(CC) -o $(@) $(^) $(LFLAGS)

clean:
	@echo cleaning...
	rm -f $(OBJ) $(TARGET)

run: build run.sh
	@printf "\n%s\n--------------------\n" "Running..."
	./run.sh

run.sh:
	@cp run_example.sh run.sh
	@chmod +x run.sh

.PHONY: build run clean
