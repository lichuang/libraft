DIR=.
BIN_DIR=$(DIR)/bin
SRC_DIR=$(DIR)/src
TEST_DIR=$(DIR)/test
INCLUDE_DIR=$(DIR)/
OBJ_DIR=$(DIR)/obj
LIB_DIR=$(DIR)/lib
LIB=libraft.a

EXTENSION=cc
OBJS=$(patsubst $(SRC_DIR)/%.$(EXTENSION), $(OBJ_DIR)/%.o,$(wildcard $(SRC_DIR)/*.$(EXTENSION)))
TEST_OBJS=$(patsubst $(TEST_DIR)/%.$(EXTENSION), $(TEST_DIR)/%.o,$(wildcard $(TEST_DIR)/*.$(EXTENSION)))
DEPS=$(patsubst $(OBJ_DIR)/%.o, $(DEPS_DIR)/%.d, $(OBJS))

INCLUDE=-I ./include -I ./src
		
CC=g++
AR= ar rcu
#CFLAGS=-std=c99 -Wall -Werror -g 
CFLAGS=-Wall -g -fno-strict-aliasing -O0 -export-dynamic -Wall -pipe  -D_GNU_SOURCE -D_REENTRANT -fPIC -Wno-deprecated -m64 
LDFLAGS= -L ./lib

all:$(OBJS)
	$(AR) $(LIB_DIR)/$(LIB) $(OBJS)

test:$(TEST_OBJS)
	$(CC) $(TEST_OBJS) test/main.cc -o test/test -lgtest -lprotobuf $(LIB_DIR)/$(LIB)

$(TEST_DIR)/%.o:$(TEST_DIR)/*test.$(EXTENSION)
	$(CC) $< -o $@ -c $(CFLAGS) $(INCLUDE) 

unstable_log_test:$(TEST_DIR)/unstable_log_test.cc
	$(CC) $< -o ./test/unstable_log_test.o -c $(CFLAGS) $(INCLUDE)  
	$(CC) ./test/unstable_log_test.o -o ./test/unstable_log_test -lgtest -lprotobuf $(LIB_DIR)/$(LIB)

$(OBJ_DIR)/%.o:$(SRC_DIR)/%.$(EXTENSION) 
	$(CC) $< -o $@ -c $(CFLAGS) $(INCLUDE) 

rebuild:
	make clean
	make

clean:
	rm -rf $(TEST_OBJS) $(OBJS) $(BIN_DIR)/* $(LIB_DIR)/*
