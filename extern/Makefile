# MIT License
#
# kvrt_bot - Telegram Bot server written in C
#
# Copyright (c) 2025 Arthur Lapz (rLapz)
#
# See LICENSE file for license details

TARGET := api

IS_DEBUG ?= 0
PREFIX   := /usr
CC       := cc
CFLAGS   := -std=c11 -Wall -Wextra -pedantic -I/usr/include/json-c -D_GNU_SOURCE
LFLAGS   := -lcurl -ljson-c -lsqlite3 -lm

SRC := api.c ../src/config.c ../src/util.c ../src/tg.c ../src/tg_api.c ../src/common.c \
	   ../src/model.c ../src/sqlite_pool.c ../src/ev.c
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

$(TARGET).o: $(TARGET).c
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


.PHONY: build clean
