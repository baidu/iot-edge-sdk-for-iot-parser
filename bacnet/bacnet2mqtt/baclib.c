/*************************************************************************
* Copyright (C) 2006 Steve Karg <skarg@users.sourceforge.net>
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*********************************************************************/

/* command line tool that sends a BACnet service, and displays the reply */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>       /* for time */

#define PRINT_ENABLED 1

#include "bacdef.h"
#include "bactext.h"
#include "bacerror.h"
#include "address.h"
#include "npdu.h"
#include "apdu.h"
#include "device.h"
#include "datalink.h"
#include "whois.h"
/* some demo stuff needed */
#include "handlers.h"
#include "client.h"
#include "dlenv.h"
#include "baclib.h"
#include "jsonutil.h"
#include "mqttutil.h"
#include "common.h"

static GlobalVar* g_vars = NULL;
static char LOG_BUFF[BUFF_LEN] = {0};

void set_global_vars(GlobalVar* pVars) {
    g_vars = pVars;
}

static void MyErrorHandler(
    BACNET_ADDRESS * src,
    uint8_t invoke_id,
    BACNET_ERROR_CLASS error_class,
    BACNET_ERROR_CODE error_code)
{
    log_debug("MyErrorHandler");
    printf("BACnet Error: %s: %s\r\n",
            bactext_error_class_name((int) error_class),
            bactext_error_code_name((int) error_code));
}

void MyAbortHandler(
    BACNET_ADDRESS * src,
    uint8_t invoke_id,
    uint8_t abort_reason,
    bool server)
{
    (void) server;
    log_debug("MyAbortHandler");
    printf("BACnet Abort: %s\r\n",
            bactext_abort_reason_name((int) abort_reason));
 }

void MyRejectHandler(
    BACNET_ADDRESS * src,
    uint8_t invoke_id,
    uint8_t reject_reason)
{
    log_debug("MyRejectHandler");
    printf("BACnet Reject: %s\r\n",
            bactext_reject_reason_name((int) reject_reason));
}

/** Handler for a ReadProperty ACK.
 * @ingroup DSRP
 * Doesn't actually do anything, except, for debugging, to
 * print out the ACK data of a matching request.
 *
 * @param service_request [in] The contents of the service request.
 * @param service_len [in] The length of the service_request.
 * @param src [in] BACNET_ADDRESS of the source of the message
 * @param service_data [in] The BACNET_CONFIRMED_SERVICE_DATA information
 *                          decoded from the APDU header of this message.
//  */
void My_Read_Property_Ack_Handler(
    uint8_t * service_request,
    uint16_t service_len,
    BACNET_ADDRESS * src,
    BACNET_CONFIRMED_SERVICE_ACK_DATA * service_data)
{
    int len = 0;
    BACNET_READ_PROPERTY_DATA data;

    log_debug("My_Read_Property_Ack_Handler");
    len = rp_ack_decode_service_request(service_request, service_len, &data);
    if (len > 0) {
        rp_ack_print_data(&data);
    }
}

PullPolicy* findPullPolicy(BACNET_ADDRESS * srcAddr, uint8_t invokeId) {
    PullPolicy* pPolicy = g_vars->g_config.policyHeader.next;
    while (pPolicy) {
        if (pPolicy->rtAddressBund == 1
            && address_match(&pPolicy->rtTargetAddress, srcAddr)
            && pPolicy->rtReqInvokeId == invokeId) {
            return pPolicy;
        }
        pPolicy = pPolicy->next;
    }

    return NULL;
}

void release_output_data(BacValueOutput* phead) {
    while (phead) {
        free(phead->value);
        free(phead->id);
        BacValueOutput* to_free = phead;
        phead = phead->next;
        free(to_free);
    }
}

const char* value_tag_to_text(uint8_t tag) {
    const char* ret = "Unknown";
    switch(tag) {
    case BACNET_APPLICATION_TAG_NULL:
        ret = "Null";
        break;
    case BACNET_APPLICATION_TAG_BOOLEAN:
        ret = "Boolean";
        break;
    case BACNET_APPLICATION_TAG_UNSIGNED_INT:
        ret = "Uint";
        break;
    case BACNET_APPLICATION_TAG_SIGNED_INT:
        ret = "Int";
        break;
    case BACNET_APPLICATION_TAG_REAL:
        ret = "Double";
        break;
    #if defined (BACAPP_DOUBLE)
    case BACNET_APPLICATION_TAG_DOUBLE:
        ret = "Double";
        break;
    #endif
        default:
        ret = "Unknown";
    }
    return ret;
}

/** Handler for a ReadPropertyMultiple ACK.
 * @ingroup DSRPM
 * For each read property, print out the ACK'd data,
 * and free the request data items from linked property list.
 *
 * @param service_request [in] The contents of the service request.
 * @param service_len [in] The length of the service_request.
 * @param src [in] BACNET_ADDRESS of the source of the message
 * @param service_data [in] The BACNET_CONFIRMED_SERVICE_DATA information
 *                          decoded from the APDU header of this message.
 */
void My_Read_Property_Multiple_Ack_Handler(
    uint8_t * service_request,
    uint16_t service_len,
    BACNET_ADDRESS * src,
    BACNET_CONFIRMED_SERVICE_ACK_DATA * service_data)
{
    int len = 0;
    BACNET_READ_ACCESS_DATA *rpm_data;
    BACNET_READ_ACCESS_DATA *old_rpm_data;
    BACNET_PROPERTY_REFERENCE *rpm_property;
    BACNET_PROPERTY_REFERENCE *old_rpm_property;
    BACNET_APPLICATION_DATA_VALUE *value;
    BACNET_APPLICATION_DATA_VALUE *old_value;
    BACNET_OBJECT_PROPERTY_VALUE object_value;

    BacValueOutput* result = NULL;
    PullPolicy* pPolicy = findPullPolicy(src, service_data->invoke_id);
    if (pPolicy != NULL) {
        rpm_data = calloc(1, sizeof(BACNET_READ_ACCESS_DATA));
        if (rpm_data) {
            len =
                rpm_ack_decode_service_request(service_request, service_len,
                rpm_data);
        }
        if (len > 0) {
            while (rpm_data) {
                object_value.object_type = rpm_data->object_type;
                object_value.object_instance = rpm_data->object_instance;

                //rpm_ack_print_data(rpm_data);
                BACNET_PROPERTY_REFERENCE *listOfProperties = rpm_data->listOfProperties;
                while (listOfProperties) {
                    BACNET_APPLICATION_DATA_VALUE *value = listOfProperties->value;
                    uint32_t valueIndex = listOfProperties->propertyArrayIndex;
                    if (listOfProperties->propertyArrayIndex == BACNET_ARRAY_ALL) {
                        valueIndex = 0;
                    }
                    while (value) {
                        valueIndex++;
                        BacValueOutput* outval = calloc(1, sizeof(BacValueOutput));
                        outval->instanceNumber = pPolicy->targetInstanceNumber;
                        outval->next = NULL;
                        outval->objectType = bactext_object_type_name(rpm_data->object_type);
                        outval->objectInstance = (int) rpm_data->object_instance;
                        outval->propertyId = bactext_property_name(listOfProperties->propertyIdentifier);
                        outval->index = valueIndex;
                        outval->type = value_tag_to_text(value->tag);

                        int buff_len = 256;
                        char buff[256];
                        {
                            object_value.object_property = listOfProperties->propertyIdentifier;
                            object_value.array_index = listOfProperties->propertyArrayIndex;
                            object_value.value = value;
                        }

                        int actLen = bacapp_snprintf_value(buff, buff_len, &object_value);
                        outval->value = (char*) malloc(actLen + 1);
                        mystrncpy(outval->value, buff, actLen);

                        int idLen = sprintf(buff, "inst_%d_%s_%d_%s_%d", outval->instanceNumber, outval->objectType, 
                            outval->objectInstance, outval->propertyId, valueIndex);

                        outval->id = (char*) malloc(idLen + 1);
                        mystrncpy(outval->id, buff, idLen + 1);

                        outval->next = result;
                        result = outval;

                        value = value->next;
                    }

                    listOfProperties = listOfProperties->next;
                }

                
                //bacapp_snprintf_value
                rpm_property = rpm_data->listOfProperties;
                while (rpm_property) {
                    value = rpm_property->value;
                    while (value) {
                        old_value = value;
                        value = value->next;
                        free(old_value);
                    }
                    old_rpm_property = rpm_property;
                    rpm_property = rpm_property->next;
                    free(old_rpm_property);
                }
                old_rpm_data = rpm_data;
                rpm_data = rpm_data->next;
                free(old_rpm_data);
            }
        } else {
            fprintf(stderr, "RPM Ack Malformed! Freeing memory...\n");
            while (rpm_data) {
                rpm_property = rpm_data->listOfProperties;
                while (rpm_property) {
                    value = rpm_property->value;
                    while (value) {
                        old_value = value;
                        value = value->next;
                        free(old_value);
                    }
                    old_rpm_property = rpm_property;
                    rpm_property = rpm_property->next;
                    free(old_rpm_property);
                }
                old_rpm_data = rpm_data;
                rpm_data = rpm_data->next;
                free(old_rpm_data);
            }
        }
    }

    if (result != NULL) {
        log_debug("received some BACNet data, going to publish it");

        BacValueOutput* nextPage = NULL;
        BacValueOutput* head = result;
        while (head != NULL) {
            // convert data into json and send to mqtt
            char* msg = bacData2Json(head, &g_vars->g_config.device, &nextPage);
            sendData(msg, g_vars);
            head = nextPage;
        }
        // release the data allocated in result
        release_output_data(result);
    }
}

static void Init_Service_Handlers(void)
{
    Device_Init(NULL);
    /* we need to handle who-is
       to support dynamic device binding to us */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);
    /* handle i-am to support binding to other devices */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, handler_i_am_bind);
    /* set the handler for all the services we don't implement
       It is required to send the proper reject message... */
    apdu_set_unrecognized_service_handler_handler
        (handler_unrecognized_service);
    /* we must implement read property - it's required! */
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROPERTY,
        handler_read_property);
    /* handle the data coming back from confirmed requests */
    apdu_set_confirmed_ack_handler(SERVICE_CONFIRMED_READ_PROPERTY,
        My_Read_Property_Ack_Handler);

    apdu_set_confirmed_ack_handler(SERVICE_CONFIRMED_READ_PROP_MULTIPLE,
        My_Read_Property_Multiple_Ack_Handler);

    /* handle any errors coming back */
    apdu_set_error_handler(SERVICE_CONFIRMED_READ_PROPERTY, MyErrorHandler);
    apdu_set_abort_handler(MyAbortHandler);
    apdu_set_reject_handler(MyRejectHandler);
}

int start_local_bacnet_device(Bac2mqttConfig* pconfig) {
    Device_Set_Object_Instance_Number(pconfig->device.instanceNumber);
    address_init();
    Init_Service_Handlers();
    dlenv_init();
    atexit(datalink_cleanup);

    return 0;
}

void receive_and_handle() {
    uint8_t Rx_Buf1[MAX_MPDU] = { 0 };
    BACNET_ADDRESS src = {
        0
    };  /* address where message came from */
    uint16_t pdu_len = 0;
    unsigned timeout = 100;     /* milliseconds */

    /* returns 0 bytes on timeout */
    pdu_len = datalink_receive(&src, &Rx_Buf1[0], MAX_MPDU, timeout);
    /* process */
    if (pdu_len) {
        snprintf(LOG_BUFF, BUFF_LEN, "datalink_receive returned %d bytes", pdu_len);
        log_debug(LOG_BUFF);
        npdu_handler(&src, &Rx_Buf1[0], pdu_len);
    }
}

int bind_bac_device_address(Bac2mqttConfig* pconfig) {
    unsigned max_apdu = 0;

    if (pconfig == NULL) {
        return -1;
    }

    PullPolicy* pNext = pconfig->policyHeader.next;
    while (pNext != NULL) {
        if (! pNext->rtAddressBund) {
            pNext->rtAddressBund =
            address_bind_request(pNext->targetInstanceNumber, &max_apdu,
            &pNext->rtTargetAddress);
            
            if (! pNext->rtAddressBund) {
                log_debug("sending WhoIs request");
                Send_WhoIs(pNext->targetInstanceNumber, pNext->targetInstanceNumber);
            }
        }
        pNext = pNext->next;
    }

    receive_and_handle();
    return 0;
}

void cleanup_read_access_data(BACNET_READ_ACCESS_DATA* header)
{
    BACNET_READ_ACCESS_DATA *rpm_object;
    BACNET_READ_ACCESS_DATA *old_rpm_object;
    BACNET_PROPERTY_REFERENCE *rpm_property;
    BACNET_PROPERTY_REFERENCE *old_rpm_property;

    rpm_object = header;
    old_rpm_object = rpm_object;
    while (rpm_object) {
        rpm_property = rpm_object->listOfProperties;
        while (rpm_property) {
            old_rpm_property = rpm_property;
            rpm_property = rpm_property->next;
            free(old_rpm_property);
        }
        old_rpm_object = rpm_object;
        rpm_object = rpm_object->next;
        free(old_rpm_object);
    }
}

int issue_read_property_multiple(PullPolicy* pPolicy) {
    // combine the multiple property request into a read_property_multiple
    if (pPolicy == NULL) {
        return -1;
    }

    if (pPolicy->propNum > 0) {
        BACNET_READ_ACCESS_DATA* header = NULL;
        int i = 0;
        for (; i < pPolicy->propNum; i++) {
            BacProperty* pProp = pPolicy->properties[i];
            BACNET_READ_ACCESS_DATA* rpm_object = calloc(1, sizeof(BACNET_READ_ACCESS_DATA));
            rpm_object->object_type = pProp->objectType;
            rpm_object->object_instance = pProp->objectInstance;

            BACNET_PROPERTY_REFERENCE* rpm_property = calloc(1, sizeof(BACNET_PROPERTY_REFERENCE));
            rpm_property->next = NULL;
            rpm_property->propertyArrayIndex = BACNET_ARRAY_ALL;    // default read all
            // we just put one property here, for simplicity
            rpm_property->propertyIdentifier = pProp->property;
            if (pProp->index >= 0) {
                rpm_property->propertyArrayIndex = pProp->index;
            }
            rpm_object->listOfProperties = rpm_property;

            rpm_object->next = header;
            header = rpm_object;
        }
        uint8_t buffer[MAX_PDU] = {0};
        log_debug("Send_Read_Property_Multiple_Request");
        pPolicy->rtReqInvokeId = Send_Read_Property_Multiple_Request(&buffer[0],
                    sizeof(buffer), pPolicy->targetInstanceNumber,
                    header);
        
        cleanup_read_access_data(header);
    }

    return 0;
}
