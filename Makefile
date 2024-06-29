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
LFLAGS   := -lcurl -ljson-c
SRC      := main.c kvrt_bot.c util.c config.c thrd_pool.c tg.c tg_api.c update_manager.c picohttpparser.c \
		    module.c builtin/anime_schedule.c builtin/general.c
OBJ      := $(SRC:.c=.o)


ifeq ($(IS_DEBUG), 1)
	CFLAGS := $(CFLAGS) -g -DDEBUG -O0
	LFLAGS := $(LFLAGS) -fsanitize=address -fsanitize=undefined
else
	CFLAGS := $(CFLAGS) -O2
endif


build: options $(TARGET)

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

run: build runner
	@printf "\n%s\n--------------------\n" "Running..."
	./run.sh

runner:
	@cp -u run_example.sh run.sh

.PHONY: build run clean
