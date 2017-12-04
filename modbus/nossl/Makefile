SOURCES = ../src/modbuslib.c ../src/common.c ../src/business.c ../src/main.c ../src/ringbufi.c ../src/mqttsender.c
HEADERS = ../src/data.h ../src/modbuslib.h ../src/common.h ../src/business.h ../src/thread.h ../src/ringbufi.h ../src/mqttsender.h
CC ?= gcc
LIBDIR ?= /usr/local/lib
INCDIR ?= /usr/local/include
DEBUGFLAG ?=
bdModbusGateway: $(SOURCES) $(HEADERS)
	$(CC) -o ../../$@ $(SOURCES) $(DEBUGFLAG) -I $(INCDIR) -L $(LIBDIR) -l:libcjson.a -lm -l:libmodbus.a -l:libpaho-mqtt3c-static.a -lpthread 

clean:
	rm ../../bdModbusGateway
