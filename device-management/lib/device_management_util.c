#include <stdio.h>
#include <log4c.h>
#include <stdlib.h>
#include "device_management_util.h"

void dump_log4c_conf() {
    FILE *fd = fopen("log4c_conf.txt", "w");
    log4c_dump_all_instances(fd);
    fclose(fd);
}

void
check_malloc_result(void *address) {
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