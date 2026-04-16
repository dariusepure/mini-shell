UTIL_PATH = util
SRC_PATH = src
CPPFLAGS += -I. -I$(SRC_PATH) -I$(UTIL_PATH)
CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -static
LDLIBS = -lreadline -lncurses -ltinfo

OBJ_PARSER = $(UTIL_PATH)/parser/parser.tab.o $(UTIL_PATH)/parser/parser.yy.o
OBJ = $(SRC_PATH)/main.o $(SRC_PATH)/cmd.o $(SRC_PATH)/utils.o
TARGET = mini-shell

.PHONY: all build clean build_parser pack

all: $(TARGET)

$(TARGET): build_parser $(OBJ) $(OBJ_PARSER)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) $(OBJ_PARSER) -o $(TARGET) $(LDLIBS)

build_parser:
	$(MAKE) -C $(UTIL_PATH)/parser/

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

pack: clean
	-rm -f ../src.zip
	zip -r ../src.zip *

clean:
	$(MAKE) -C $(UTIL_PATH)/parser/ clean
	-rm -f ../src.zip
	-rm -rf $(OBJ) $(TARGET) *~
