SOURCES = ../src/*.c
HEADERS = ../src/*.h
CC ?= gcc
LIBDIR ?= /usr/local/lib
INCDIR ?= /usr/local/include
DEBUGFLAG ?=
bdModbusGateway.exe: $(SOURCES) $(HEADERS)
	$(CC) -o ../../$@ $(SOURCES) $(DEBUGFLAG) -I $(INCDIR) -L $(LIBDIR) -Wl,--no-as-needed -lm   -l:libssl.a -l:libcrypto.a  -l:libcjson.a -l:libmodbus.a -l:libpaho-mqtt3cs.dll -l:libssl.a -l:libcrypto.a -lws2_32

clean:
	rm ../../bdModbusGateway.exe
	rm ../../libpaho-mqtt3cs.dll
