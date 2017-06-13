#ifndef TEST_CONF_H
#define TEST_CONF_H

#include <string>

class TestConf {
public:
    static const std::string testMqttBroker;
    static const std::string testMqttUsername;
    static const std::string testMqttPassword;

    static const std::string topicPrefix;
};

#endif //TEST_CONF_H