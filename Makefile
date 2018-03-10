DIR=.
BIN_DIR=$(DIR)/bin
SRC_DIR=$(DIR)/src
INCLUDE_DIR=$(DIR)/
OBJ_DIR=$(DIR)/obj
LIB_DIR=$(DIR)/lib
LIB=libraft.a

EXTENSION=cc
OBJS=$(patsubst $(SRC_DIR)/%.$(EXTENSION), $(OBJ_DIR)/%.o,$(wildcard $(SRC_DIR)/*.$(EXTENSION)))
DEPS=$(patsubst $(OBJ_DIR)/%.o, $(DEPS_DIR)/%.d, $(OBJS))

INCLUDE=-I ./include 
		
CC=g++
AR= ar rcu
#CFLAGS=-std=c99 -Wall -Werror -g 
CFLAGS=-Wall -g -fno-strict-aliasing -O0 -export-dynamic -Wall -pipe  -D_GNU_SOURCE -D_REENTRANT -fPIC -Wno-deprecated -m64 
#LDFLAGS= -L ./lib -lcr -pthread
LDFLAGS= -L ./lib -lcr -ldl -lpthread

all:$(OBJS)
	$(AR) $(LIB_DIR)/$(LIB) $(OBJS)

$(OBJ_DIR)/%.o:$(SRC_DIR)/%.$(EXTENSION) 
	$(CC) $< -o $@ -c $(CFLAGS) $(INCLUDE) 

rebuild:
	make clean
	make

clean:
	rm -rf $(OBJS) $(BIN_DIR)/* $(LIB_DIR)/*
