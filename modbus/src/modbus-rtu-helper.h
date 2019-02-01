#ifndef MODBUS_RTU_HELPER_H
#define MODBUS_RTU_HELPER_H

#include <ctype.h>
#include <modbus/modbus.h>
#include "data.h"
#include "modbus-rtu-helper.h"

typedef struct slave_id_helper {
    int slaveid;
    struct slave_id_helper* next;
} slave_id_helper_t;

typedef struct ctx_share_helper {
    int baud;
    int databits;
    char parity;
    int stopbits;
    modbus_t* share_ctx;
    slave_id_helper_t* slave_list;
} ctx_share_helper_t;

int check_params(SlavePolicy *p1, ctx_share_helper_t *p2);

int extract_index(const char* device);

ctx_share_helper_t* create_ctx_share_helper(int baud, int databits, char parity, int stopbits, modbus_t* share_ctx);

void release_ctx_share_helper(ctx_share_helper_t* helper);

void add_slave(int slaveid, ctx_share_helper_t* helper);

void remove_slave(int slaveid, ctx_share_helper_t* helper);

slave_id_helper_t* create_slave_id_helper(int slaveid);

void release_slave_id_helper_list(slave_id_helper_t* slaveid_helper);

#endif  /* MODBUS_RTU_HELPER_H */