SOURCES = ../src/modbuslib.c ../src/common.c ../src/business.c ../src/main.c
HEADERS = ../src/data.h ../src/modbuslib.h ../src/common.h ../src/business.h
bdModbusGateway: $(SOURCES) $(HEADERS)
	gcc -o ../$@ $(SOURCES) -lcjson -lm -lmodbus -lpaho-mqtt3cs -lpthread 

clean:
	rm ../bdModbusGateway
