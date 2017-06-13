//
// Created by zhaobo03 on 17-6-8.
//

#ifndef DEVICE_MANAGEMENT_DEVICEMANAGEMENTSTUB_H
#define DEVICE_MANAGEMENT_DEVICEMANAGEMENTSTUB_H

#include <string>
#include <memory>
#include <bits/shared_ptr.h>

class DeviceManagementStubImpl;

class DeviceManagementStub {
public:
    typedef std::function<void (const std::string &, const std::string &)> CallBack;

    static std::shared_ptr<DeviceManagementStub> create();

    virtual void start() = 0;
    virtual void setAutoResponse(bool value) = 0;
    virtual void addListener(CallBack f) = 0;
    virtual void clearListeners() = 0;
};

#endif //DEVICE_MANAGEMENT_DEVICEMANAGEMENTSTUB_H
