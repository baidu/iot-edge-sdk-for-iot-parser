//
// Created by zhaobo03 on 17-6-8.
//

#include "DeviceManagementStub.h"
#include <regex>
#include <mqtt/client.h>
#include <cjson/cJSON.h>
#include <boost/format.hpp>

static const std::string topicPrefix = "$baidu";

class DeviceManagementStubImpl: public mqtt::callback {
public:
    DeviceManagementStubImpl(const std::string &broker, const std::string &username, const std::string &password,
    const std::string clientId);
    void start();

    static const std::string update;

private:
    std::string username;
    std::string password;
    mqtt::client client;

    static const std::regex topicRegex;
    static const boost::format acceptedFormat;
    static const boost::format rejectedFormat;

    // Overrides
    void connection_lost(const std::string& cause) override;
    void message_arrived(const std::string& topic, mqtt::const_message_ptr msg) override;
    void delivery_complete(mqtt::idelivery_token_ptr tok) override;

    // Business logics
    void processUpdate(const std::string &device, const std::string requestId, cJSON *document);
};

std::string DeviceManagementStubImpl::update = "update";



// First group matches device name and second group matches action.
std::regex DeviceManagementStubImpl::topicRegex = std::regex("baidu/iot/shadow/(.*)/(.*)");



DeviceManagementStubImpl::DeviceManagementStubImpl(const std::string &broker, const std::string &username,
                                                   const std::string &password, const std::string clientId):
        client(broker, clientId), username(username), password(password) {

}

void DeviceManagementStubImpl::start() {
    mqtt::connect_options options;
    options.set_user_name(username);
    options.set_password(password);
    client.set_callback(*this);
    client.connect(options);
}

void DeviceManagementStubImpl::message_arrived(const std::string &topic, mqtt::const_message_ptr msg) {
    std::match_results results;
    bool matched = std::regex_match(topic.data(), results, topicRegex, std::regex_constants::match_default);
    if (matched) {
        const std::string &device = results[1];
        const std::string &action = results[2];
        if (action == DeviceManagementStubImpl::update) {

        }
    }
}

void DeviceManagementStubImpl::processUpdate(const std::string &device, const std::string requestId, cJSON *document) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "requestId", requestId.data());
    mqtt::message message();
}