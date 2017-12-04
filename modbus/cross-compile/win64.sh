#!/bin/bash

SCRIPT=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT")
echo $BASEDIR

# 0, prepare tools
echo "0, prepare tools. you need following tools be insstalled first:"
echo "sudo  apt-get --yes --force-yes install gcc-mingw-w64-x86-64"
echo "sudo  apt-get --yes --force-yes install cmake"
echo "sudo  apt-get --yes --force-yes install g++-mingw-w64-x86-64"
echo "sudo  apt-get --yes --force-yes install autoconf"
echo "sudo  apt-get --yes --force-yes install libtool"
echo "sudo  apt-get --yes --force-yes install binutils-mingw-w64-i686 "

G_CC=x86_64-w64-mingw32-gcc
G+CPP=x86_64-w64-mingw32-g++
G_RANLIB=x86_64-w64-mingw32-ranlib
G_LD=x86_64-w64-mingw32-LD

# 4, make a temp dir
echo "4, make a temp dir"
DEPS=deps_linux_win64
mkdir $DEPS
cd $DEPS
mkdir cmake
mkdir output
OUTPUTDIR=$BASEDIR/$DEPS/output
DEPSDIR=$BASEDIR/$DEPS

# 5, download and install cJSON
echo "5, download and install cJSON"
if [ -f $OUTPUTDIR/lib/libcjson.a ]
then
    echo "$OUTPUTDIR/lib/libcjson.a exist, skipping cjson compilation"
else
wget https://github.com/DaveGamble/cJSON/archive/v1.5.9.tar.gz
tar zxvf v1.5.9.tar.gz
cd cmake
rm -rf *
cmake -DCMAKE_C_COMPILER=$G_CC -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=x86_64 -DCMAKE_SYSTEM_VERSION=1 -DBUILD_SHARED_LIBS=Off -DCMAKE_INSTALL_PREFIX=$OUTPUTDIR  -DENABLE_CJSON_TEST=FALSE ../cJSON-1.5.9/
cmake --build .
make install
fi

# 6, download and install libmodbus
echo "6, download and install libmodbus"
if [ -f $OUTPUTDIR/lib/libmodbus.a ]
then
    echo "$OUTPUTDIR/lib/libmodbus.a exist, skipping libmodbus compilation"
else
cd $DEPSDIR
wget https://github.com/stephane/libmodbus/archive/v3.1.4.tar.gz
tar zxvf v3.1.4.tar.gz
cd libmodbus-3.1.4
./autogen.sh
./configure CC=$G_CC RANLIB=$G_RANLIB LD=$G_LD --host=i686 --enable-static=yes  --prefix=$OUTPUTDIR ac_cv_func_malloc_0_nonnull=yes --without-documentation
make install
fi


# 7, download and install OpenSSL
echo "7, download and install OpenSSL"
if [ -f $OUTPUTDIR/lib/libssl.a ]
then
    echo "$OUTPUTDIR/lib/libssl.a exist, skipping openssl compilation"
else
cd $DEPSDIR
wget https://github.com/openssl/openssl/archive/OpenSSL_1_1_0f.tar.gz
tar zxvf OpenSSL_1_1_0f.tar.gz
cd openssl-OpenSSL_1_1_0f
./Configure mingw64 no-shared
make CC=$G_CC RANLIB=$G_RANLIB LD=$G_LD MAKEDEPPROG=$G_CC PROCESSOR=X86_64
cp libssl.a libcrypto.a $OUTPUTDIR/lib
cp -r include/openssl/ $OUTPUTDIR/include 
fi

# 8, download and install paho.mqtt.c
echo "8, download and install paho.mqtt.c"
PAHO_MQTT_VER=1.2.1
if [ -f $OUTPUTDIR/lib/libpaho-mqtt3cs.dll ]
then
    echo "$OUTPUTDIR/lib/libpaho-mqtt3cs.dll, skipping paho.mqtt.c compilation"
else
cd $DEPSDIR
wget https://github.com/ubyyj/paho.mqtt.c/archive/v$PAHO_MQTT_VER.tar.gz -O v$PAHO_MQTT_VER.tar.gz
tar zxvf v$PAHO_MQTT_VER.tar.gz
cd cmake
rm -rf *
# disable test
echo > ../paho.mqtt.c-$PAHO_MQTT_VER/test/CMakeLists.txt
cmake -DCMAKE_C_COMPILER=$G_CC -DCMAKE_CXX_COMPILER=$G_CPP -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=x86_64 -DCMAKE_SYSTEM_VERSION=1 -DPAHO_WITH_SSL=TRUE -DPAHO_BUILD_STATIC=TRUE -DOPENSSL_SEARCH_PATH=$OUTPUTDIR -DOPENSSLCRYPTO_LIB=$OUTPUTDIR/lib/libcrypto.a -DOPENSSL_LIB=$OUTPUTDIR/lib/libssl.a  -DOPENSSL_INCLUDE_DIR=$OUTPUTDIR/include -DPAHO_BUILD_SAMPLES=FALSE  -DCMAKE_RC_COMPILER_ENV_VAR=RC -DCMAKE_RC_COMPILER="" -DCMAKE_SHARED_LINKER_FLAGS="-fdata-sections -ffunction-sections -Wl,--enable-stdcall-fixup -static-libgcc -static " -DCMAKE_EXE_LINKER_FLAGS="-fdata-sections -ffunction-sections -Wl,--enable-stdcall-fixup -static-libgcc -static "   ../paho.mqtt.c-$PAHO_MQTT_VER/

# add "-lcrypt32" to the end of the last line in link.txt
sed '${s/$/ -lcrypt32/}' src/CMakeFiles/paho-mqtt3cs.dir/link.txt > tmp
cp tmp src/CMakeFiles/paho-mqtt3cs.dir/link.txt
sed '${s/$/ -lcrypt32/}' src/CMakeFiles/paho-mqtt3as.dir/link.txt > tmp
cp tmp  src/CMakeFiles/paho-mqtt3as.dir/link.txt
cmake --build .
cp src/libpaho-mqtt3cs.dll src/libpaho-mqtt3as-static.a $OUTPUTDIR/lib
cp ../paho.mqtt.c-$PAHO_MQTT_VER/src/MQTTAsync.h ../paho.mqtt.c-$PAHO_MQTT_VER/src/MQTTClient.h ../paho.mqtt.c-$PAHO_MQTT_VER/src/MQTTClientPersistence.h $OUTPUTDIR/include
fi

# 8, make Baidu Iot Edge SDK
cd $BASEDIR
cp Makefile-win Makefile
make LIBDIR=$OUTPUTDIR/lib INCDIR=$OUTPUTDIR/include CC=$G_CC
cp $OUTPUTDIR/lib/libpaho-mqtt3cs.dll ../../
cp ../../bdModbusGateway.exe ../bin/win64
echo "======================================="
echo "SUCCESS, executable is located at ../../bdModbusGateway.exe"
