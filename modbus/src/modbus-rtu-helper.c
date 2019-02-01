#include "modbus-rtu-helper.h"

char* g_com_prefix_1 = "COM";
char* g_com_prefix_2 = "\\\\.\\COM";

/* baud, data_bit ,stop_bit ,parity */
int check_params(SlavePolicy *p1, ctx_share_helper_t *p2)
{
    if (p1->baud == p2->baud
        && p1->databits == p2->databits
        && p1->parity == p2->parity
        && p1->stopbits == p2->stopbits)
    {
        return 0;
    }
    return -1;
}

/* required "COMxx", xx being a decimal number and 0 <= xx <= 254 */
int extract_index(const char* device)
{
    char prefix[10];
    memset(prefix, '\0', sizeof(prefix));

    int len = strlen(device);
    if (len == 4)
    {
        /* COM0 ~ COM9 */
        strncpy(prefix, device, 3);
        if (strcmp(prefix, g_com_prefix_1) != 0)
        {
            return -1;
        }
        char ch = device[3];
        if (isdigit(ch))
        {
            return ch - '0';
        }
    }
    else if (len >= 9)
    {
        /* \\.\COM10 ~ \\.\COM254 */
        strncpy(prefix, device, 7);
        if (strcmp(prefix, g_com_prefix_2) != 0)
        {
            return -1;
        }
        int result = 0;
        int i = 7;
        for (i = 7; i < len; i++)
        {
            char ch = device[i];
            if (isdigit(ch))
            {
                result = result * 10 + (ch - '0');
            }
            else
            {
                return -1;
            }
        }
        if (result < 10 || result > 254)
        {
            return -1;
        }
        return result;
    }
    return -1;
}

ctx_share_helper_t* create_ctx_share_helper(int baud, int databits, char parity, int stopbits, modbus_t* share_ctx)
{
    ctx_share_helper_t* helper = (ctx_share_helper_t*) malloc(sizeof(ctx_share_helper_t));
    helper->baud = baud;
    helper->databits = databits;
    helper->parity = parity;
    helper->stopbits = stopbits;
    helper->share_ctx = share_ctx;
    helper->slave_list = NULL;
}

void release_ctx_share_helper(ctx_share_helper_t* helper)
{
    if (helper == NULL)
    {
        return;
    }
    release_slave_id_helper_list(helper->slave_list);
    free(helper);
}

void add_slave(int slaveid, ctx_share_helper_t* helper)
{
    if (helper == NULL)
    {
        return;
    }
    slave_id_helper_t* next = helper->slave_list;
    slave_id_helper_t* target = NULL;
    while(next != NULL && target == NULL)
    {
        if (next->slaveid == slaveid)
        {
            target = next;
        }
        next = next->next;
    }
    if (target == NULL)
    {
        target = create_slave_id_helper(slaveid);
        target->next = helper->slave_list;
        helper->slave_list = target;
    }
}

void remove_slave(int slaveid, ctx_share_helper_t* helper)
{
    if (helper == NULL)
    {
        return;
    }
    slave_id_helper_t* last = NULL;
    slave_id_helper_t* next = helper->slave_list;
    slave_id_helper_t* target = NULL;
    while(next != NULL && target == NULL)
    {
        if (next->slaveid == slaveid)
        {
            slave_id_helper_t* target = next;
            if (last == NULL)
            {
                // target is first node
                helper->slave_list = target->next;
            }
            else
            {
                last->next = target->next;
            }
        }
        last = next;
        next = next->next;
    }
    if (target != NULL)
    {
        free(target);
    }
}

slave_id_helper_t* create_slave_id_helper(int slaveid)
{
    slave_id_helper_t* slaveid_helper = (slave_id_helper_t*) malloc(sizeof(slave_id_helper_t));
    slaveid_helper->slaveid = slaveid;
    slaveid_helper->next = NULL;
    return slaveid_helper;
}

void release_slave_id_helper_list(slave_id_helper_t* slaveid_helper)
{
    slave_id_helper_t* target = NULL;
    while(slaveid_helper != NULL)
    {
        target = slaveid_helper;
        slaveid_helper = slaveid_helper->next;
        free(target);
    }
}