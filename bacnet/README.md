百度BACNet网关(beta)
====================

介绍
---

百度BACNet网关是一个端上的程序，需要运行在用户BACNet设备所在的网络上的计算机或者开发板上。它模拟成一个BACNet IP设备，并且根据配置，采集网络内其他BACNet设备的数据，并且上传到指定的物接入主题。
它主要基于bacnet-stack-0.8.3开发，并且依赖paho.mqtt.c, cJSON等库。

安装
----
bin目录下有已经针对常见系统，预编译好了可执行程序。如果你的系统包含在内，你可以直接使用可执行文件，而不是自己编译。
目前针对如下平台做了预编译：
* linux_x86
* win32

如果你需要自己编译代码，请参考bacnet2mqtt/linux_x86.sh。

如果你希望在linux下交叉编译，请参考bacnet2mqtt/win32.sh。

使用步骤
-------
在成功编译好bdBacnetGateway之后，请通过如下步骤使用：

1，登录百度物接入(IoT Hub)，选择或者创建一套：实例、设备、身份、策略。策略中指定2个主题，一个用于下发采集策略(比如取名bacConfigTopic)，一个用于网关上传BACNet数据（比如取名bacDataTopic)，得到如下信息：
* `1) 实例地址`
* `2) 用户名`
* `3) 密码`
* `4) 配置下发主题`
* `5) 数据上传主题`

2，在bdBacnetGateway同级目录下，创建名为gwconfig-bacnet.txt的文件，文件的格式如下：
```
{
    "endpoint": "<填步骤1中的实例地址>",
    "configTopic": "<填步骤1中的配置下发主题>",
    "dataTopic": "<填步骤1中的数据上传主题>",
    "user": "<填步骤1中的用户名>",
    "password": "<填步骤1中的密码>"
}
```

下面是一个具体的文件内容的例子：
```
{
    "endpoint": "tcp://yyj.mqtt.iot.gz.baidubce.com:1883",
    "configTopic": "configTopic",
    "dataTopic": "dataTopic",
    "user": "yyj/thing",
    "password": "1rsf1wkjdifeljKij89fHLCIYp3sjOPO5FxoxTjPFGjyU="
}
```

3，运行bdBacnetGateway： ```sudo ./bdBacnetGateway```

4，往配置下发MQTT主题发布BACNet数据采集策略。下面是数据采集策略的一个实例：
```
{
    "bdBacVer": 1,
    "device": {
        "instanceNumber": 134,
        "ipOrInterface": null
    },
    "pullPolices": [
        {
            "targetInstanceNumber": 2,
            "interval": 5,
            "properties": [
                {
                    "objectType": "ANALOG_INPUT",
                    "objectInstance": 1,
                    "property": "PRESENT_VALUE"
                },
                {
                    "objectType": "ANALOG_OUTPUT",
                    "objectInstance": 1,
                    "property": "PRESENT_VALUE"
                }
            ]
        }
    ]
}
```

**device.instanceNumber**为本网关使用的instanceNumber，需要指定一个与其他BACNet设备不同的instanceNumber，以免冲突。
**pullPolices**为真正的采集策略，是一个数组，可以提供多个采集策略。
**pullPolices**中的每一个元素，表示针对某个特定的BACNet设备以某个特定的频率，采集一个或者多个属性。**targetInstanceNumber**为被采集的BACNet设备的instanceNumber，**interval**为采集间隔(秒)。**properties**为需要采集的属性列表，分别指定了对象类型，对象instaceNumber，以及属性ID。

发送MQTT消息，可以通过物接入设备旁边的**测试连接**工具，或者mqttfx桌面工具，进行发送。发送BACNet采集策略，建议设置retain标志为true。

除了通过发送MQTT消息的方式外，你也可以把上述的数据采集策略，保存在bdBacnetGateway同级目录下面的，名为policyCache-bacnet.txt的文件中。

5，这时候，bdBacnetGateway应该能接受（或者读取）到数据采集策略，并且按照指定的间隔采集数据，并且将数据发布到步骤1中的数据上传主题。你可以通过订阅这个主题，检查数据是否正确上传。数据上传的格式示例如下：
```
{
    "bdBacVer": 1,
    "device":   {
        "instanceNumber":   134,
        "ip":   null,
        "broadcastIp":  null
    },
    "ts":   1493369528,
    "data": [{
            "id":   "inst_2_analog-input_1_present-value_1",
            "instance": 2,
            "objType":  "analog-input",
            "objInstance":  1,
            "propertyId":   "present-value",
            "index":    4294967295,
            "type": "Double",
            "value":    "0.00000"
        }, {
            "id":   "inst_2_analog-output_1_present-value_1",
            "instance": 2,
            "objType":  "analog-output",
            "objInstance":  1,
            "propertyId":   "present-value",
            "index":    4294967295,
            "type": "Double",
            "value":    "4.00000"
        }]
}
```

如果需要将上传的数据写入时序数据库(TSDB)的话，可以基于dataTopic创建规则引擎，并且使用如下SQL查询语句：
```
*, 'data' AS _TSDB_META.data_array, 'value' AS _TSDB_META.value_field, 'ts' AS _TSDB_META.global_time, 'id' AS _TSDB_META.point_metric, 'device.instanceNumber' AS _TSDB_META.global_tags.tag1, 'instance' AS _TSDB_META.point_tags.tag1, 'objType' AS _TSDB_META.point_tags.tag2, 'objInstance' AS _TSDB_META.point_tags.tag3, 'propertyId' AS _TSDB_META.point_tags.tag4
```
