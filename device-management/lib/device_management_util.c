/*
* Copyright (c) 2016 Baidu, Inc. All Rights Reserved.
*
* Licensed to the Apache Software Foundation (ASF) under one or more
* contributor license agreements.  See the NOTICE file distributed with
* this work for additional information regarding copyright ownership.
* The ASF licenses this file to You under the Apache License, Version 2.0
* (the "License"); you may not use this file except in compliance with
* the License.  You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

/**
 * @brief internal helper functions.
 *
 * @authors Zhao Bo
 */
#include <stdio.h>
#include <log4c.h>
#include <stdlib.h>
#include "device_management_util.h"

void dump_log4c_conf() {
    FILE *fd = fopen("log4c_conf.txt", "w");
    log4c_dump_all_instances(fd);
    fclose(fd);
}

void check_malloc_result(void *address) {
    if (address == NULL) {
        exit(EXIT_FAILURE);
    }
}

void safe_free(char **pointer) {
    if (pointer != NULL && *pointer != NULL) {
        free(*pointer);
        *pointer = NULL;
    }
}