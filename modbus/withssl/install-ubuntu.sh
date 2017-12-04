#!/bin/bash

SCRIPT=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT")
echo $BASEDIR

# 0, make sure gcc version >= 4.9
gccver=$(gcc --version | grep ^gcc | sed 's/^.* //g')
echo "gcc version:${gccver}"
if [[ ${gccver} < 4.9 ]]; then
	sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
	sudo apt-get update
	sudo apt-get --yes --force-yes install gcc-4.9

	sudo ln -f -s /usr/bin/gcc-4.9 /usr/bin/gcc
else
	echo "gcc version is OK!"
fi

# 1, install git
echo "1, install git"
sudo apt-get --yes --force-yes install git

# 2, install auto conf
echo "2, install auto conf"
sudo apt-get --yes --force-yes install autoconf

# 3, install libtool
echo "3, install libtool"
sudo apt-get --yes --force-yes install libtool

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
cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=x86 -DCMAKE_SYSTEM_VERSION=1 -DBUILD_SHARED_LIBS=Off -DCMAKE_INSTALL_PREFIX=$OUTPUTDIR  -DENABLE_CJSON_TEST=FALSE ../cJSON-1.5.9/
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
./configure CC=gcc --enable-static=yes  --prefix=$OUTPUTDIR
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
./Configure linux-generic32
make
cp libssl.a libcrypto.a $OUTPUTDIR/lib
cp -r include/openssl/ $OUTPUTDIR/include 
fi

# 8, download and install paho.mqtt.c
echo "8, download and install paho.mqtt.c"
if [ -f $OUTPUTDIR/lib/libpaho-mqtt3cs-static.a ]
then
    echo "$OUTPUTDIR/lib/libpaho-mqtt3cs-static.a exist, skipping paho.mqtt.c compilation"
else
cd $DEPSDIR
wget https://github.com/eclipse/paho.mqtt.c/archive/v1.2.0.tar.gz
tar zxvf v1.2.0.tar.gz
cd cmake
rm -rf *
# disable test
echo > ../paho.mqtt.c-1.2.0/test/CMakeLists.txt
cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_VERSION=1 -DPAHO_WITH_SSL=TRUE -DPAHO_BUILD_STATIC=TRUE -DOPENSSL_SEARCH_PATH=$OUTPUTDIR -DOPENSSLCRYPTO_LIB=$OUTPUTDIR/lib/libcrypto.a -DOPENSSL_LIB=$OUTPUTDIR/lib/libssl.a  -DOPENSSL_INCLUDE_DIR=$OUTPUTDIR/include ../paho.mqtt.c-1.2.0/
cmake --build .
cp src/libpaho-mqtt3a-static.a src/libpaho-mqtt3c-static.a src/libpaho-mqtt3cs-static.a src/libpaho-mqtt3as-static.a $OUTPUTDIR/lib
cp ../paho.mqtt.c-1.2.0/src/MQTTAsync.h ../paho.mqtt.c-1.2.0/src/MQTTClient.h ../paho.mqtt.c-1.2.0/src/MQTTClientPersistence.h $OUTPUTDIR/include
fi

# 8, make Baidu Iot Edge SDK
cd $BASEDIR
make LIBDIR=$OUTPUTDIR/lib INCDIR=$OUTPUTDIR/include
cp ../../bdModbusGateway ../bin/linux_x86/bdModbusGateway
echo "======================================="
echo "SUCCESS, executable is located at ../../bdModbusGateway"
