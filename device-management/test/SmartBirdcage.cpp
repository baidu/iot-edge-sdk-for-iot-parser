//
// Created by zhaobo03 on 17-6-8.
//

#include <algorithm>
#include "SmartBirdcage.h"

SmartBirdcage::SmartBirdcage(DeviceManagementClient c, uint32_t maxWater): c(c), maxWater(maxWater) {
    waterMark = 0;
}

uint32_t SmartBirdcage::addWater(uint32_t value) {
    uint32_t actual = std::min(value, waterMark);
    waterMark -= actual;
    return actual;
}