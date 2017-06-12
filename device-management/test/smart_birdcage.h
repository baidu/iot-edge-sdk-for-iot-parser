//
// Created by zhaobo03 on 17-6-8.
//

#ifndef DEVICE_MANAGEMENT_SMARTBIRDCAGE_H
#define DEVICE_MANAGEMENT_SMARTBIRDCAGE_H

#include <device_management.h>

class SmartBirdcage {
private:
    const uint32_t maxWater;
    uint32_t waterMark;
    const DeviceManagementClient c;
public:
    SmartBirdcage(DeviceManagementClient c, uint32_t maxWater);
    uint32_t drinkWater(uint32_t expected);
    uint32_t addWater(uint32_t value);
};

#endif //DEVICE_MANAGEMENT_SMARTBIRDCAGE_H
