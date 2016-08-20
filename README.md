Baidu modbus gateway
====================

Overview
--------

Baidu modbus gateway is a client program designed to run under user's 
corporation network. It samples modbus data from user's modbus slave
devices, and upload into the Baidu modbus parser service, to be parsed
and archived.

It receives admins configuration from the cloud via MQTT message, then
execute the sampling tasks specified in the config, finally upload
the modbus package to cloud service, via MQTT message again.

Once Baidu modbus parser service receives a modbus package, it parse
the package according parse rules user specified, so the data becomes
readable.

Finally, parsed modbus package is archived into BOS, for later use.

Installation
------------

Though this program could work with SSL, people may not need it in
order to prompt speed and reduce disk requirement, hence we have two
install guide, for with and without SSL.

For people who want SSL, please refer to withssl/install.txt.
For people who don't want SSL, please refer to nossl/install.txt

Documentation
-------------

A more detailed step by step guide could be found at [here](https://cloud.baidu.com/doc/Parser/index.html)

