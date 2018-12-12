Baidu modbus gateway
====================

Overview
--------

Baidu modbus gateway is a client program designed to run under user's 
corporation network. It samples modbus data from user's modbus slave
devices, and upload into the Baidu modbus parser service, to be parsed
and archived.

百度modbus网关是一个端上的程序，需要运行在用户设备的现场。它采集用户modbus从站的数据，并且上传到百度物解析服务，然后根据解析项目配置的解析设置，进行解析。最后入库。

It receives admins configuration from the cloud via MQTT message, then
execute the sampling tasks specified in the config, finally upload
the modbus package to cloud service, via MQTT message again.

他通过定义订阅MQTT主题以接受管理配置（采集策略），然后执行相关的采集任务，采集到数据后，依然通过MQTT协议上数据上传到云端。

Once Baidu modbus parser service receives a modbus package, it parse
the package according parse rules user specified, so the data becomes
readable.

云端接受到数据后，根据用户在云端配置的解析设置，将数据解析成明文数据。

Finally, parsed modbus package is archived into BOS or MQTT topic, for later use.

最后，解析后的数据存入BOS，或者另外一个MQTT主题，以便进行后续处理或使用。

Installation
------------

There are pre-compiled executables under bin directory for a few popular platforms, you may just pick one if any one fits your environemnt, instead of compiling it yourself.
We have pre-compiled executables for following platforms:
* linux_x86
* linux_arm
* win64
* win32

bin目录下有已经针对常见系统，预编译好了可执行程序。如果你的系统包含在内，你可以直接使用可执行文件，而不是自己编译。
目前针对如下平台做了预编译：
* linux_x86
* linux_arm
* win64
* win32

Though this program could work with SSL, people may not need it in
order to prompt speed and reduce disk requirement, hence we have two
install guide, for with and without SSL.
虽然该程序可以使用SSL与百度物接入通信，但你也可以选择不使用SSL加密通信，以减少对SSL库的依赖，以及减少计算量。所以我们有两个安装向导，分别对应需要SSL，和不需要SSL的场景。

For people who want SSL, please refer to withssl/ubuntu-install.sh. Run it like: sudo /bin/bash ubuntu-install.sh
对于需要SSL的用户，请参考withssl/ubuntu-install.sh。像这样运行: ```sudo /bin/bash ubuntu-install.sh```

For people who don't want SSL, please refer to nossl/ubuntu-install.txt
对于不需要SSL的用户，请参考nossl/ubuntu-install.sh。像这样运行: ```sudo /bin/bash ubuntu-install.sh```

Cross compilation
-----------------
A few cross compilation scripts are located under foler cross-compile, which are provided for your reference. They are tested under ubuntu, and require a few build tools are installed first, which are describted at the beginning of each script.

在cross-compile目录下面，提供了几个常见运行平台的交叉编译脚本，仅供参考。他们在ubuntu下面测试过，并且它们要求系统已经安装了一批编译工具。详细情况在每个脚本的开头有描述。

Documentation
-------------

A more detailed step by step guide could be found at [here](https://cloud.baidu.com/doc/Parser/index.html)
物解析服务详细的使用文档，请参考[这里](https://cloud.baidu.com/doc/Parser/index.html)

使用步骤
--------
在成功编译好bdModbusGateway之后，请通过如下步骤使用：

1，登录百度天工物解析服务，新建一个网关。新建完成之后，点击网关右侧的**查看密钥**链接，复制对话框中全部内容。

2，按照[文档](https://cloud.baidu.com/doc/Parser/index.html)的提示，继续完成：
* `网关下面的子设备的创建`
* `解析项目的创建` （如果希望解析后的数据存入时序数据库，请填写一个**目的地主题**，并且点击**快速创建存储至TSDB的规则引擎**一键创建)
* `解析设置的创建`
* `轮询规则的创建`

3，回到你的网关程序所在的目录，在bdModbusGateway同级目录下，创建名为gwconfig.txt的文件，并将第1步复制的网关密钥粘贴进文件。文件的格式如下：
```
{
"endpoint":"ssl://parser_endpoint1473673432475.mqtt.iot.gz.baidubce.com:1884",
"topic":"mb_commandTopic_v21493783120844",
"user":"parser_endpoint1473673432475/mb_thing_v21493783120844",
"password":"gnPeOWRsoNfakedDonotUsesobJY2+c+VU1UJAAbBnjI=",
"backControlTopic":"mb_backControlTopic_1493783120844"
}
```
如果需要网关上报额外的字段（比如设备的imei号等，用来区分不同的物理网关），可以在gwconfig.txt中添加名为misc的对象，其内容可以为任何有效的JSON格式。该网关会在每个上传云端的数据包带上misc对象，并且在解析后的消息中输出。如下所示：
```
{
    "endpoint": "ssl://parser_endpoint1473673432475.mqtt.iot.gz.baidubce.com:1884",
    "topic": "mb_commandTopic_v21493783120844",
    "user": "parser_endpoint1473673432475/mb_thing_v21493783120844",
    "password": "gnPeOWRsoNfakedDonotUsesobJY2+c+VU1UJAAbBnjI=",
    "backControlTopic": "mb_backControlTopic_1493783120844",
    "misc": {
        "imei": "20171113",
        "anyotherkey": "anyothervalue"
    }
}
```

4，运行bdModbusGateway: ```./bdModbusGateway```

5，点击解析项目或者网关页面里面的**全部生效**按钮。至此，所有需要你操作的步骤已经完成，其他事情系统自动会完成。

在后台，系统会把数据采集策略，通过gwconfig.txt中的topic主题下发给网关，网关会将采集策略保存在policyCache.txt文件中，并且开始调度数据采集任务。采集到的数据，会通过采集策略里面指定的mqtt主题上传到天工云端。上传的数据格式如下：
```
{
    "bdModbusVer": 1,
    "gatewayid": "b7905963-c954-41b9-b53c-7cb4c1c8518a",
    "trantable": "74633ecc-3de2-49d0-abd2-4058f2589426",
    "modbus": {
        "request": {
            "functioncode": 3,
            "slaveid": 1,
            "startAddr": 0,
            "length": 2
        },
        "response": "00000000",
        "parsedResponse": null,
        "error": null
    },
    "timestamp": "1505560528"
}
```
**modbus.request**为采集modbus使用的命令参数。

**modbus.response**为网关采集到的原始modbus数据。

**modbus.parsedResponse**为空，后面经过云端解析后，会填上。

如果你自己需要带上额外的信息，可以修改该SDK，在上传的数据中，加上**misc**字段，该字段格式不限，解析后原样输出。例如，你可以做这里带上设备的其他固有属性。

在云端解析之后，会变成如下格式（填上了modbus.parsedResponse和metrics字段）:
```
{
    "bdModbusVer": 1,
    "gatewayid": "b7905963-c954-41b9-b53c-7cb4c1c8518a",
    "trantable": "74633ecc-3de2-49d0-abd2-4058f2589426",
    "modbus": {
        "request": {
            "functioncode": 3,
            "slaveid": 1,
            "startAddr": 0,
            "length": 2
        },
        "response": "00000000",
        "parsedResponse": [
            {
                "desc": "chiller pressure",
                "type": "INT",
                "unit": "",
                "value": "1",
                "errno": 0
            },
            {
                "desc": "water flow",
                "type": "INT",
                "unit": "",
                "value": "2",
                "errno": 0
            }
        ],
        "error": null
    },
    "timestamp": "1505560528",
    "metrics": {
        "chiller pressure": 1,
        "water flow": 2
    },
    "misc": null
}
```
解析后的数据，根据解析项目里面的设置，会存入BOS或者目的地主题，如果对接了规则引擎写TSDB，后面还会写入TSDB。

如果手工创建规则引擎将解析后的数据写入TSDB，请参考使用如下SQL查询语句：
```
 *, 'metrics' AS  _TSDB_META_v2.metric_nodes.node1, 'timestamp' AS _TSDB_META_v2.ts, 'modbus.request.functioncode' AS _TSDB_META_v2.tags.tag1, 'modbus.request.slaveid' _TSDB_META_v2.tags.tag2, 'gatewayid' _TSDB_META_v2.tags.tag3, 'yyyy-MM-dd HH:mm:ssZ' AS _TSDB_META_v2.time_format 
```

反控
----
当需要反控的时候，就向backControlTopic发送一条反控指令，反控指令为JSON格式，示例如下：
```
{
    "request1": {
        "slaveid": 1,
        "address": 1,
        "data": "0101010100000000"
    }，
    "request2": {
        "slaveid": 2,
        "address": 40001,
        "data": "00ff1234"
    }
}
```
一个消息可以包含多个指令，分别用requestx(x为数字编号,1,2,3…n)来表示。slaveid为需要反控的modbus从站编号, address为需要写的寄存器的起始地址，data为要写往modbus从站的数据，从address开始，依次往后写。如上面的request1，会向地址1-8等8个离散值(coins)写数据，写入的值分别为1,1,1,1,0,0,0,0； 上面的request2，会写2个寄存器，40001和40002，写入的值分别为00ff, 1234。

需要说明的是，如果backControlTopic为"mb_backControlTopic_1493783120844"，那么网关对"mb_backControlTopic_1493783120844/#"都是有权限的。也就是说，在gwconfig.txt文件中，把backControlTopic设置成如下主题，都是可以的：
```
"backControlTopic": "mb_backControlTopic_1493783120844"
"backControlTopic": "mb_backControlTopic_1493783120844/a"
"backControlTopic": "mb_backControlTopic_1493783120844/b"
...
"backControlTopic": "mb_backControlTopic_1493783120844/anystring"
```

离线缓存
-------
在网关没有网络的情况下，默认会将采集到的Modbus数据缓存到本地名为data_cache.dat的文件中，最大缓存500MB最新的数据。如果需要修改最大缓存大小，请先删除data_cache.dat文件，然后在gwconfig.txt文件中增加一项名为cacheSize的配置，如下所示，将大小设置成3000000字节：
```
{
    ...
    "cacheSize": 3000000
}
```

断线监控
-------
为了指示网关的工作状态，方便监控进程判断网关的工作状态，网关在每次成功地采集数据或者发送数据时，将当前时间写入到对应的文件中去。
1，当数据采集成功(成功地从Modbus从站读取到数据)，会将当前时间戳写入到如下文件：
on_modbus_read
2，当成功将数据通过MQTT协议发送到云端，会将当前时间戳写入到如下文件：
on_mqtt_sent