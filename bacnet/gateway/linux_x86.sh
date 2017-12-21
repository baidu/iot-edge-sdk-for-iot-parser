#!/bin/bash

SCRIPT=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT")
echo $BASEDIR

# 0, prepare tools
echo "0, prepare tools. you need following tools be insstalled first:"
echo "sudo  apt-get --yes --force-yes install i686-w64-mingw32-gcc"
echo "sudo  apt-get --yes --force-yes install cmake"
echo "sudo  apt-get --yes --force-yes install i686-w64-mingw32-g++"
echo "sudo  apt-get --yes --force-yes install autoconf"
echo "sudo  apt-get --yes --force-yes install libtool"
echo "sudo  apt-get --yes --force-yes install binutils-mingw-w64-i686 "

G_CC=gcc
G_CPP=g++
G_RANLIB=ranlib
G_AR=ar
G_LD=LD

# 4, make a temp dir
echo "4, make a temp dir"
DEPS=deps_linux_x86
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
cd $DEPSDIR
wget https://github.com/DaveGamble/cJSON/archive/v1.5.9.tar.gz -O v1.5.9.tar.gz
tar zxvf v1.5.9.tar.gz
cd cmake
rm -rf *
cmake -DCMAKE_C_COMPILER=$G_CC -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=x86 -DCMAKE_SYSTEM_VERSION=1 -DBUILD_SHARED_LIBS=Off -DCMAKE_INSTALL_PREFIX=$OUTPUTDIR  -DENABLE_CJSON_TEST=FALSE ../cJSON-1.5.9/
cmake --build .
make install
fi

# 6, download and install OpenSSL
echo "6, download and install OpenSSL"
if [ -f $OUTPUTDIR/lib/libssl.a ]
then
    echo "$OUTPUTDIR/lib/libssl.a exist, skipping openssl compilation"
else

cd $DEPSDIR
wget https://github.com/openssl/openssl/archive/OpenSSL_1_1_0f.tar.gz -O OpenSSL_1_1_0f.tar.gz
tar zxvf OpenSSL_1_1_0f.tar.gz
cd openssl-OpenSSL_1_1_0f
./Configure linux-generic32 no-shared
make CC=$G_CC RANLIB=$G_RANLIB LD=$G_LD MAKEDEPPROG=$G_CC PROCESSOR=X86
cp libssl.a libcrypto.a $OUTPUTDIR/lib
cp -r include/openssl/ $OUTPUTDIR/include 
fi

# 7, download and install paho.mqtt.c
echo "7, download and install paho.mqtt.c"
if [ -f $OUTPUTDIR/lib/libpaho-mqtt3cs-static.a ]
then
    echo "$OUTPUTDIR/lib/libpaho-mqtt3cs-static.a exist, skipping paho.mqtt.c compilation"
else

cd $DEPSDIR
wget https://github.com/eclipse/paho.mqtt.c/archive/v1.2.0.tar.gz -O v1.2.0.tar.gz
tar zxvf v1.2.0.tar.gz
cd cmake
rm -rf *
# disable test
echo > ../paho.mqtt.c-1.2.0/test/CMakeLists.txt
cmake -DCMAKE_C_COMPILER=$G_CC -DCMAKE_CXX_COMPILER=$G_CPP -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=x86 -DCMAKE_SYSTEM_VERSION=1 -DPAHO_WITH_SSL=TRUE -DPAHO_BUILD_STATIC=TRUE -DOPENSSL_SEARCH_PATH=$OUTPUTDIR -DOPENSSLCRYPTO_LIB=$OUTPUTDIR/lib/libcrypto.a -DOPENSSL_LIB=$OUTPUTDIR/lib/libssl.a  -DOPENSSL_INCLUDE_DIR=$OUTPUTDIR/include -DPAHO_BUILD_SAMPLES=FALSE  ../paho.mqtt.c-1.2.0/

cmake --build .
cp src/libpaho-mqtt3cs-static.a $OUTPUTDIR/lib
cp ../paho.mqtt.c-1.2.0/src/MQTTAsync.h ../paho.mqtt.c-1.2.0/src/MQTTClient.h ../paho.mqtt.c-1.2.0/src/MQTTClientPersistence.h $OUTPUTDIR/include
fi

# 8, make Baidu Iot Edge SDK
cd $BASEDIR
make clean
make IOT_LIB_DIR=$OUTPUTDIR/lib IOT_INC_DIR=$OUTPUTDIR/include CC=$G_CC AR=$G_AR BUILD=release
cp bdBacnetGateway ../bin/linux_x86/bdBacnetGateway
echo "======================================="
echo "SUCCESS, executable is located at ../bin/linux_x86/bdBacnetGateway"
