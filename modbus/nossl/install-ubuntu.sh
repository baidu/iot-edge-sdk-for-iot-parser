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
mkdir ~/deps

# 5, download and install cJSON
echo "5, download and install cJSON"
cd ~/deps
git clone https://github.com/DaveGamble/cJSON.git
cd cJSON
make
sudo make install

# 6, download and install libmodbus
echo "6, download and install libmodbus"
cd ~/deps
git clone https://github.com/stephane/libmodbus.git
cd libmodbus 
./autogen.sh 
./configure 
sudo make install

# 7, download and install paho.mqtt.c
echo "7, download and install paho.mqtt.c"
cd ~/deps
git clone https://github.com/eclipse/paho.mqtt.c.git
cp paho.mqtt.c/src/VersionInfo.h.in paho.mqtt.c/src/VersionInfo.h
cp $BASEDIR/paho_Makefile_c paho.mqtt.c/Makefile
cd paho.mqtt.c 
sudo make install

# 8, make the install libs take effect
sudo ldconfig

# 9, make Baidu Iot Edge SDK
cd $BASEDIR
make

echo "======================================="
echo "SUCCESS, executable is located at ../../bdModbusGateway"
