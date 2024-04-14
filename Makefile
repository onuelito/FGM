CC=gcc


CFLAGS=-c -Wall -MD -MMD -Iinclude/ -std=c99
LDFLAGS=-lX11 -lGL -lm

BIN_DIR=bin
SRC_DIR=src
OBJ_DIR=obj

SRC_FILES=$(wildcard $(SRC_DIR)/*c $(SRC_DIR)/**/*.c)
OBJ_FILES=$(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%,$(SRC_FILES:.c=.o))
DEP_FILES=$(OBJ_FILES:.o=.d)

BIN=$(BIN_DIR)/app

all: $(BIN)

$(BIN): $(OBJ_FILES)
	mkdir -p $(@D)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.c
	mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -rf $(BIN_DIR)/* $(OBJ_DIR)/*

-include $(DEP_FILES)
