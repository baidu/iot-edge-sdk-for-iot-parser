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
BACNET_LIB=-L$(BACNET_LIB_DIR),-l$(BACNET_LIB_NAME)
ifeq (${BACNET_PORT},linux)
PFLAGS = -pthread
TARGET_EXT =
SYSTEM_LIB=-lc,-lgcc,-lrt,-lm
endif
ifeq (${BACNET_PORT},bsd)
PFLAGS = -pthread
TARGET_EXT =
SYSTEM_LIB=-lc,-lm
endif
ifeq (${BACNET_PORT},win32)
TARGET_EXT = .exe
SYSTEM_LIB=-lws2_32,-lgcc,-lm,-liphlpapi,-lwinmm
DEFINES += -D_NO_OLDNAMES
endif
#build for release (default) or debug
DEBUGGING =
OPTIMIZATION = -Os
ifeq (${BUILD},debug)
OPTIMIZATION = -O0
DEBUGGING = -g
endif

LFLAGS_IOT = -lcjson -lm -lpaho-mqtt3c
ifeq (${WITHSSL},yes)
LFLAGS_IOT = -lcjson -lm -lpaho-mqtt3cs
endif

# put all the flags together
CFLAGS := -Wall $(DEBUGGING) $(OPTIMIZATION) $(INCLUDES) $(DEFINES)
LFLAGS := -Wl,$(BACNET_LIB),$(SYSTEM_LIB),${LFLAGS_IOT}



# Executable file name
TARGET = bdBacnetGateway
TARGET_BIN = ${TARGET}$(TARGET_EXT)

SRCS = $(wildcard *.c) \
	../bacnet-stack/demo/object/device-client.c \

HEADERS = $(wildcard *.h)

OBJS = ${SRCS:.c=.o}

all: ${BACNET_LIB_TARGET} Makefile ${TARGET_BIN}

${TARGET_BIN}: ${OBJS} Makefile ${BACNET_LIB_TARGET} $(HEADERS)
	${CC} ${PFLAGS} ${OBJS} ${LFLAGS} -o $@  
	size $@
	cp $@ ../../

lib: ${BACNET_LIB_TARGET}

${BACNET_LIB_TARGET}:
	( cd ${BACNET_LIB_DIR} ; $(MAKE) clean ; $(MAKE) )

.c.o:
	${CC} -c ${CFLAGS} $*.c -o $@
 
depend:
	rm -f .depend
	${CC} -MM ${CFLAGS} *.c >> .depend

clean:
	rm -rf core ${TARGET_BIN} ${OBJS}  ../../${TARGET_BIN}

include: .depend
