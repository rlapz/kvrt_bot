# MIT License
#
# kvrt_bot - Telegram Bot written in C
#
# Copyright (c) 2024 Arthur Lapz (rLapz)
#
# See LICENSE file for license details


TARGET    := kvrt_bot
BUILD_DIR := ./build
SRC_DIRS  := ./src

SRCS       := $(shell find $(SRC_DIRS) -name '*.c')
OBJS       := $(SRCS:%=$(BUILD_DIR)/%.o)
INC_DIRS   := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS  := $(addprefix -I,$(INC_DIRS))
CCFLAGS    := -std=c11 -Wall -Wextra -pedantic -I/usr/include/json-c -D_POSIX_C_SOURCE=200112L
LDFLAGS    := -lcurl -ljson-c -lsqlite3
TARGET_BIN := $(BUILD_DIR)/$(TARGET)
RELEASE    ?= 0

ifeq ($(RELEASE), 1)
	CCFLAGS := $(CCFLAGS) -O2
else
	CCFLAGS := $(CCFLAGS) -g -DDEBUG -O0
	LDFLAGS := $(LDFLAGS) -fsanitize=address -fsanitize=undefined
endif


build: options $(TARGET_BIN) run.sh

options:
	@echo \'$(TARGET)\' build options:
	@echo "CFLAGS =" $(CCFLAGS)
	@echo "CC     =" $(CC)

$(TARGET_BIN): $(OBJS)
	@printf "\n%s\n--------------------\n" "Linking..."
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(INC_FLAGS) $(CCFLAGS) -c $< -o $@

run: build run.sh
	@printf "\n%s\n--------------------\n" "Running..."
	./run.sh

run.sh:
	@cp run_example.sh run.sh
	@chmod +x run.sh

.PHONY: clean run
clean:
	rm -rf $(BUILD_DIR)
