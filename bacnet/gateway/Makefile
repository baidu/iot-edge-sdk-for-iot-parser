#Makefile to build BACnet Application using GCC compiler

# tools - only if you need them.
# Most platforms have this already defined
# CC = gcc
# AR = ar
# MAKE = make
# SIZE = size
#
# Assumes rm and cp are available

# Directories
BACNET_PORT ?= linux
BACNET_PORT_DIR = ../bacnet-stack/ports/${BACNET_PORT}
BACNET_INCLUDE = ../bacnet-stack/include
BACNET_OBJECT = ../bacnet-stack/demo/object
BACNET_HANDLER = ../bacnet-stack/demo/handler
# BACnet Library
BACNET_LIB_DIR = ../bacnet-stack/lib
BACNET_LIB_NAME = bacnet
BACNET_LIB_TARGET = $(BACNET_LIB_DIR)/lib$(BACNET_LIB_NAME).a
# Compiler Setup
INCLUDE1 = -I$(BACNET_PORT_DIR) -I$(BACNET_OBJECT) -I$(BACNET_HANDLER)
INCLUDE2 = -I$(BACNET_INCLUDE)
INCLUDES = $(INCLUDE1) $(INCLUDE2)
BACNET_LIB=-L$(BACNET_LIB_DIR) -l$(BACNET_LIB_NAME)
ifeq (${BACNET_PORT},linux)
PFLAGS = -pthread
TARGET_EXT =
SYSTEM_LIB=-lc -lgcc -lrt -lm -ldl
endif
ifeq (${BACNET_PORT},bsd)
PFLAGS = -pthread
TARGET_EXT =
SYSTEM_LIB=-lc,-lm
endif
ifeq (${BACNET_PORT},win32)
TARGET_EXT = .exe
SYSTEM_LIB=-lws2_32 -lgcc -lm -liphlpapi -lwinmm
DEFINES += -D_NO_OLDNAMES
endif
#build for release (default) or debug
DEBUGGING =
OPTIMIZATION = -Os
ifeq (${BUILD},debug)
OPTIMIZATION = -O0
DEBUGGING = -g
endif

#arch
CPU_ARCH = 
ifeq (${ARCH},amd64)
CPU_ARCH = -m64
endif

PAHO_LIB =libpaho-mqtt3cs-static.a
ifeq (${BACNET_PORT},win32)
  PAHO_LIB =libpaho-mqtt3cs.dll
endif

IOT_LIB_DIR ?= /usr/local/lib
LFLAGS_IOT =-L${IOT_LIB_DIR} -l:libcjson.a -l:$(PAHO_LIB) -l:libssl.a -l:libcrypto.a

IOT_INC_DIR ?=/usr/local/include

# put all the flags together
CFLAGS := -Wall $(DEBUGGING) $(OPTIMIZATION) $(INCLUDES) $(DEFINES) -I $(IOT_INC_DIR) $(CPU_ARCH)
LFLAGS := $(LFLAGS_IOT) $(BACNET_LIB) $(SYSTEM_LIB) 


# Executable file name
TARGET = bdBacnetGateway
TARGET_BIN = ${TARGET}$(TARGET_EXT)

SRCS = $(wildcard *.c) \
	../bacnet-stack/demo/object/device-client.c \

HEADERS = $(wildcard *.h)

OBJS = ${SRCS:.c=.o}

all: ${BACNET_LIB_TARGET} Makefile ${TARGET_BIN}

${TARGET_BIN}: ${OBJS} Makefile ${BACNET_LIB_TARGET} $(HEADERS)
	${CC} ${PFLAGS}  ${OBJS}  -o $@ ${LFLAGS} $(CPU_ARCH)
	size $@

lib: ${BACNET_LIB_TARGET}

${BACNET_LIB_TARGET}:
	( cd ${BACNET_LIB_DIR}/.. ; $(MAKE) clean ; $(MAKE) STANDARDS=$(CPU_ARCH) BACNET_PORT=$(BACNET_PORT) library)

.c.o:
	${CC} -c ${CFLAGS}  $*.c -o $@
 
depend:
	rm -f .depend
	${CC} -MM ${CFLAGS} *.c >> .depend

clean:
	rm -rf core ${TARGET_BIN} ${OBJS}  ../../${TARGET_BIN} bdBacnetGateway* ../../libpaho-mqtt* ../bacnet-stack/src/bip.o ../bacnet-stack/src/bvlc.o ../bacnet-stack/ports/*/bip-init.o
	 ( cd ${BACNET_LIB_DIR} ; $(MAKE) clean)
 
include: .depend
