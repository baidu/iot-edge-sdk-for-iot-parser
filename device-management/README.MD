# Baidu IoT Edge SDK / Device Management

## Installation
### Ubuntu 64bit
如果您使用的是Ubuntu 64bit操作系统，可以这里下载我们发布的安装包：https://github.com/baidu/iot-edge-sdk-for-iot-parser/releases

下载所有的zip文件并unzip之后：
```bash
sudo apt-get install uuid-dev libssl-dev
sudo dpkg -i libcjson_1.5.2-1_amd64.deb liblog4c_1.2.4-1_amd64.deb libpaho-mqtt-c_1.1.0-1_amd64.deb libbaiduiot-dm_1.0.1-1_amd64.deb
```

## Building from source
### CMake & GNU/Linux
#### Build requirement
构建依赖以下工具：
* CMake
* gcc

此外，还需要以下库：
* paho-mqtt-c (https://github.com/eclipse/paho.mqtt.c) - MQTT客户端
* log4c (http://log4c.sourceforge.net/) - 记录日志
* cJson (https://github.com/DaveGamble/cJSON) - Json序列化
* uuid - 生成requestId

如果执行测试，还需要以下库：
* gtest
* gmock
* libboost-dev

在Ubuntu上，可以用命令安装部分依赖。
```bash
apt-get install build-essential gcc make cmake cmake-gui cmake-curses-gui uuid-dev
```
对于paho-mqtt-c，log4c，cJson，则需要我们从源码编译和安装。请点击对应链接，参考具体的构建指南。如果您使用的是Ubuntu 64bit操作系统，也从这里下载安装包：https://github.com/baidu/iot-edge-sdk-for-iot-parser/releases

#### Build instructions
```bash
# 
mkdir build
cd build
cmake ../
cd lib
make
sudo make install
```

## Libraries

这个 SDK 包含一个动态链接库:
 * libbaidu-iot-dm.so

## Usage and API
```
#include <device_management.h>
#include <cjson.h>

static int acceptedAck = 0;
static int rejectedAck = 0;
static int timeoutAck = 0;

void shadow_action_callback(ShadowAction action, ShadowAckStatus status, ShadowActionAck *ack, void *context) {
    if (status == SHADOW_ACK_ACCEPTED) {
        acceptedAck++;
    } else if (status == SHADOW_ACK_REJECTED) {
        rejectedAck++;
    } else if (status == SHADOW_ACK_TIMEOUT) {
        timeoutAck++;
    }
}

int main() {
    DmReturnCode rc;
    const char *broker = "ssl://samples.mqtt.iot.gz.baidubce.com:1884";
    const char *username = "test/test"; /* 设置为您设备的用户名 */
    const char *password = "test"; /* 设置为您设备的密码 */
    const char *deviceName = "pump1"; /* 设置为您设备的名字 */

    device_management_init();

    DeviceManagementClient client;
    /* 设置为正确的root_cert.pem路径. git repository根目录有一份该文件。*/
    rc = device_management_create(&client, broker, deviceName, username, password, NULL, "./root_cert.pem");
    rc = device_management_connect(client);

    cJSON *reported = cJSON_CreateObject();

    cJSON *frequencyIn = cJSON_CreateNumber(20);
    cJSON *current = cJSON_CreateNumber(111.0);
    cJSON *speed = cJSON_CreateNumber(1033);
    cJSON *torque = cJSON_CreateNumber(41.5);
    cJSON *power = cJSON_CreateNumber(31.9);
    cJSON *dcBusVoltage = cJSON_CreateNumber(543);
    cJSON *outputVoltage = cJSON_CreateNumber(440);
    cJSON *driveTemp = cJSON_CreateNumber(40);

    cJSON_AddItemToObject(reported, "FrequencyIn", frequencyIn);
    cJSON_AddItemToObject(reported, "Current", current);
    cJSON_AddItemToObject(reported, "Speed", speed);
    cJSON_AddItemToObject(reported, "Torque", torque);
    cJSON_AddItemToObject(reported, "Power", power);
    cJSON_AddItemToObject(reported, "DC_bus_voltage", dcBusVoltage);
    cJSON_AddItemToObject(reported, "Output_voltage", outputVoltage);
    cJSON_AddItemToObject(reported, "Drive-temp", driveTemp);

    rc = device_management_shadow_update(client, shadow_action_callback, NULL, 10, reported);
    // 设备影子的更新是异步的,device_management_shadow_update返回并不等于已经收到服务器端的响应.
    // 在这个示例中,我们在一个loop中等待收到ACK.
    while (acceptedAck + rejectedAck + timeoutAck < 1) {
        sleep(1);
    }

    cJSON_Delete(reported);

    rc = device_management_destroy(client);

    rc = device_management_fini();
}
```
以上示例代码来源自samples/pump.c

## Logging
SDK使用log4c来记录日志，category名为device-management。可以通过调整log4c的配置来控制日志输出。
参见 samples/log4crc。
```xml
<?xml version="1.0" encoding="ISO-8859-1"?>
<!DOCTYPE log4c SYSTEM "">
<log4c>
        <config>
                <bufsize>0</bufsize>
                <debug level="0"/>
                <nocleanup>0</nocleanup>
        </config>
        <!-- root category ========================================= -->
        <category name="root" priority="trace" appender="stdout"/>
        <category name="device-management" priority="trace" appender="stdout"/>
        <!-- default appenders ===================================== -->
        <appender name="stdout" type="stream" layout="dated"/>
        <appender name="stderr" type="stream" layout="dated"/>
        <appender name="syslog" type="syslog" layout="basic"/>
        <!-- default layouts ======================================= -->
        <layout name="basic" type="basic"/>
        <layout name="dated" type="dated"/>
</log4c>

```