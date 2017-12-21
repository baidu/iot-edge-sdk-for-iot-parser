#include "data.h"
#include "thread.h"

PullPolicy* new_pull_policy()
{
    PullPolicy* ret = (PullPolicy*) malloc(sizeof(PullPolicy));
    ret->rtAddressBund = 0;
    ret->rtReqInvokeId = 0;
    ret->next = NULL;
    ret->propNum = 0;
    return ret;
}

BacProperty* new_bac_property()
{
    BacProperty* ret = (BacProperty*) malloc(sizeof(BacProperty));
    ret->index = -1;
    return ret;
}

BacObject* new_bac_object()
{
    BacObject* ret = (BacObject*) malloc(sizeof(BacObject));
    ret->need_to_read = 0;
    ret->support_cov = UN_SUPPORT_SUB;
    ret->un_support_state = UN_SUPPORT_SUB;
    ret->subscribe_time = 0;
    ret->last_read_time = 0;
    ret->next = NULL;
    return ret;
}

BacDevice2* new_bac_device2()
{
    BacDevice2* ret = (BacDevice2*) malloc(sizeof(BacDevice2));
    ret->disable = 1;
    ret->send_next = 1;
    ret->update_objects = 0;
    ret->objects_size = 0;
    ret->next_index = 0;
    ret->last_ack_time = 0;
    ret->request_list.head = NULL;
    ret->request_list.tail = NULL;
    ret->request_list_mutex = Thread_create_mutex();
    ret->current_state = INIT_STATE;
    ret->objects_header = NULL;
    ret->handle_next_object = NULL;
    return ret;
}