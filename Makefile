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
		    module/anime_schedule.c module/general.c
OBJ      := $(SRC:.c=.o)


ifeq ($(IS_DEBUG), 1)
	CFLAGS := $(CFLAGS) -g -DDEBUG -O0
	LFLAGS := $(LFLAGS) -fsanitize=address -fsanitize=undefined
else
	CFLAGS := $(CFLAGS) -O2
endif


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
