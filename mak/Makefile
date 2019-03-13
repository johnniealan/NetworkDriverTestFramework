# Author: Johnnie Alan
#ROOT_DIR=../
ROOT_DIR:= $(shell pwd)/..

DEMO_TEST_MASTER_LIB = $(ROOT_DIR)/bin/masterDemoTstCase.so
DEMO_TEST_SLAVE_LIB = $(ROOT_DIR)/bin/slaveDemoTstCase.so
#DEMO_TEST_STAND_LIB = $(ROOT_DIR)/bin/standlaoneDemoTstCase.so
TEST_FW = $(ROOT_DIR)/bin/testFW
CONSOLE = $(ROOT_DIR)/src/console.c
SLAVE = $(ROOT_DIR)/src/slave.c
MASTER = $(ROOT_DIR)/src/master.c
STANDALONE = $(ROOT_DIR)/src/standalone.c

CFLAGS += -I../inc -I/usr/include/ -I/usr/local/include/libxml2/ #-I/usr/include/libxml2/

#XML parser
XML_LIB = -lxml2

#Addtional Lib
ADD_LIB=-ldl -lpthread -lpanel -lncurses

#Timer
TIMER_LIB = -lrt

LDFLAGS = -L$(ROOT_DIR)/bin/

CC=gcc -g
RM=rm -fr

ifeq ($(host),arm)
CC=arm-linux-gnueabihf-gcc
LDFLAGS = -L$(ROOT_DIR)/bin/ -lpanel -lncurses #-lConsole_arm
SHARED_LIB_FLAGS = -fPIC -DPIC 
XML_LIB += -lz #-l:libConsole_arm.a
endif

all:$(TEST_FW) $(DEMO_TEST_MASTER_LIB) $(DEMO_TEST_SLAVE_LIB)

$(TEST_FW): $(ROOT_DIR)/src/deviceDrvTestFW.c 
	$(CC) $< $(CONSOLE) $(STANDALONE) $(SLAVE) $(MASTER) $(CFLAGS) $(LDFLAGS) $(XML_LIB) $(ADD_LIB) $(TIMER_LIB) -o  $@

$(DEMO_TEST_MASTER_LIB): $(ROOT_DIR)/testcases/demo/masterTstCase.c
	$(CC) $(CFLAGS) $(SHARED_LIB_FLAGS) -shared $< -o $@

$(DEMO_TEST_SLAVE_LIB): $(ROOT_DIR)/testcases/demo/slaveTstCase.c
	$(CC) $(CFLAGS) $(SHARED_LIB_FLAGS) -shared $< -o $@


clean:
	$(RM) $(TEST_FW) $(DEMO_TEST_MASTER_LIB) $(DEMO_TEST_SLAVE_LIB) $(DEMO_TEST_STAND_LIB)

