# BACnet网关

简介
----

该网关可以发现BACnet IP设备，并且自动去采集所有被发现的BACnet设备的对象。采集到的数据通过Restful API，以及MQTT协议两种方式暴露给用户。网关支持多种采集模式，如定期轮训(readPropertyMultiple)、订阅对象(subscribeCOV)、订阅属性(subscribeCOVProperty)，订阅属性并指定COV_INCREMENT(subscribeCOVProperty)。

除了采集BACnet设备的数据，还可以通过云端反控BACnet设备的状态。

使用步骤
--------

**1, 下载网关**
 网关可执行程序位于bin目录下，提供如下4种平台的可执行文件：
* linux_amd64
* linux_arm
* linux_x86
* win32

**2, 创建云端网关**
* 登录百度云 https://console.bce.baidu.com
* 访问BACnet网关管理页面：https://console.bce.baidu.com/iot2/bacnet/
* 点击添加网关。在新建网关界面，填写网关名称；如有需要可以调整其他选填参数。例如，如果运行网关程序的计算机有多个网卡，那么需要在网卡或IP字段填写BACnet设备所在的网络对应的网卡名或IP。Linux环境填网卡名，如eth0；Windows环境填网卡对应的IP，如192.168.93.1。
* 点击保存
* 在网关列表页面，点击网关右边的连接配置
* 在连接配置对话框中点击右下角的下载按钮，一个名为gwconfig-bacnet.txt的文件会被下载到本地

**3, 现场部署** 
将网关可执行程序与gwconfig-bacnet.txt部署到现场，启动网关程序

**4, 云端查看BACnet设备及对象列表**

如果网关发现所在的网络有其他的BACnet设备，则会把这些设备、以及设备里面的对象都采集到云端。

返回到网关管理页面，点击网关右边的BACnet设备链接，该页面会显示网关在现场发现的所有BACnet设备。点击每个设备右边的获取对象列表链接，会显示该BACnet设备上面的采集了的对象，及其当前状态。

获取数据
--------
网关采集的数据，除了通过Restful API获取，还可以通过订阅MQTT主题来实时获取。

在网关、BACnet设备、BACnet对象上，均可以设置数据目的地主题。例如在网关上设置了数据目的地主题topic1，那么这个网关下面所有设备下面的所有对象数据的更新，都会发送到topic1。其格式如下：
~~~~~~~~~
{
    "gateway": 1,
    "instanceNumber": 10825,
    "objectType": "ANALOG_INPUT",
    "objectInstance": 1,
    "presentValue": 15.2,
    "reliability": "GOOD",
    "name": "ai2",
    "ts": 1507706493,
    "previous": {
        "presentValue": 14,
        "ts": 1507705239
    },
    "props": {
        "ANALOG_INPUT_1": 15
    }
}
~~~~~~~~~
如果需要将数据存入时序数据库(TSDB)，可以创建规则引擎，以topic1为输入主题，以如下SQL语句为查询字段：
~~~~~~~~~
objectType as metric, presentValue as _value, ts as _timestamp, gateway, instanceNumber, objectInstance, name
~~~~~~~~~
