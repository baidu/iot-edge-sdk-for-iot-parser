//
// Created by zhaobo03 on 17-6-12.
//

#include <uuid/uuid.h>
#include "test_util.h"

std::string TestUtil::uuid() {
    uuid_t uuid;
    char uuidString[64];
    uuid_generate(uuid);
    uuid_unparse(uuid, uuidString);
    return std::string(uuidString);
}