//
// Created by zhaobo03 on 17-6-8.
//

#ifndef DEVICE_MANAGEMENT_DEVICEMANAGEMENTSTUB_H
#define DEVICE_MANAGEMENT_DEVICEMANAGEMENTSTUB_H

#include <string>
#include <bits/shared_ptr.h>
#include <bits/unique_ptr.h>

class ShadowEventListener {
public:
    virtual void onAction() = 0;
};

class DeviceManagementStub {
private:
    const std::string broker;
    const std::string username;
    const std::string password;

public:
    void disableAutoResponse();
    void addListener(ShadowEventListener listener);
};


#endif //DEVICE_MANAGEMENT_DEVICEMANAGEMENTSTUB_H
