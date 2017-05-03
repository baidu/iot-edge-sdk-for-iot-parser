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
最后，解析后的数据存入BOS，或者另外一个MQTT主题，一遍进行后续处理或使用。

Installation
------------

Though this program could work with SSL, people may not need it in
order to prompt speed and reduce disk requirement, hence we have two
install guide, for with and without SSL.
虽然该程序可以使用SSL与百度物接入通信，但你也可以选择不使用SSL加密通信，以减少对SSL库的依赖，以及减少计算量。所以我们有两个安装向导，分别对应需要SSL，和不需要SSL的场景。

For people who want SSL, please refer to withssl/ubuntu-install.sh. Run it like: sudo /bin/bash ubuntu-install.sh
对于需要SSL的用户，请参考withssl/ubuntu-install.sh。像这样运行: ```sudo /bin/bash ubuntu-install.sh```

For people who don't want SSL, please refer to nossl/ubuntu-install.txt
对于不需要SSL的用户，请参考nossl/ubuntu-install.sh。像这样运行: ```sudo /bin/bash ubuntu-install.sh```

Documentation
-------------

A more detailed step by step guide could be found at [here](https://cloud.baidu.com/doc/Parser/index.html)
物解析服务详细的使用文档，请参考[这里](https://cloud.baidu.com/doc/Parser/index.html)

