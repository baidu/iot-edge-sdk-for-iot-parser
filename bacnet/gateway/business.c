#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "common.h"
#include "jsonutil.h"
#include "mqttutil.h"
#include "baclib.h"
#include "data.h"
#include "bactext.h"

const char* const CONFIG_FILE = "gwconfig-bacnet.txt";
const char* const POLICY_CACHE = "policyCache-bacnet.txt";

GlobalVar g_vars;
char g_buff[BUFF_LEN];
int g_stop_worker = 0;
int g_worker_is_running = 0;

char g_cov_increment_change = 0;
char g_subscribe_type_change = 0;
SUBSCRIBE_STATE g_cancel_type = UN_NEED_TO_CANCEL;

void reset_gateway();

void load_mqtt_config(const char* file, MqttInfo* pInfo)
{
    printf("starting to load gateway mqtt config from file %s\n", CONFIG_FILE);

    char* content = NULL;
    long size = read_file_as_string(file, &content);
    if (size <= 0)
    {
        printf("failed to open config file %s, while trying to load gateway configuration\n",
               file);
        exit(1);
    }

    json2_mqtt_info(content, pInfo);

}

void connection_lost(void* context, char* cause)
{
    printf("\nConnection lost, caused by %s, will reconnect later\n", cause);
    // Thread_lock_mutex((g_vars.g_gateway_mutex));
    g_vars.g_gateway_connected = 0;
    // Thread_unlock_mutex((g_vars.g_gateway_mutex));
}

void delivered(void* context, MQTTClient_deliveryToken dt)
{
    printf("Message with token value %d delivery confirmed\n", dt);
}

void msg_arrived_config(cJSON* root)
{
    if(cJSON_HasObjectItem(root, "stop"))
    {
        g_stop_worker = json_bool(root, "stop");
    }

    if (cJSON_HasObjectItem(root, "instanceNumber"))
    {
        // the first config
        if (g_vars.g_config.rtConfLoaded == 0)
        {
            g_vars.g_config.device.instanceNumber = (uint32_t) json_int(root, "instanceNumber");
            g_vars.g_config.device.ipOrInterface = NULL;
            g_vars.g_config.rtConfLoaded = 1;
        }
    }

    if (cJSON_HasObjectItem(root, "ipOrInterface"))
    {
        cJSON* ip = cJSON_GetObjectItem(root, "ipOrInterface");
        if (ip != NULL && !cJSON_IsNull(ip))
        {
            char* ip_str = ip->valuestring;
            if(g_vars.g_config.device.ipOrInterface == NULL
                || strcmp(g_vars.g_config.device.ipOrInterface, ip_str) != 0)
            {
                // a new value for ipOrInterface
                int len = strlen(ip_str) + 1;
                if(g_vars.g_config.device.ipOrInterface != NULL)
                {
                    free(g_vars.g_config.device.ipOrInterface);
                }
                g_vars.g_config.device.ipOrInterface = (char*) malloc(sizeof(char)*len);
                snprintf(g_vars.g_config.device.ipOrInterface, len, "%s", ip_str);
                // restart the gateway
                g_vars.g_config.rtDeviceStarted = 0;
            }
        }
    }

    if (cJSON_HasObjectItem(root, "pollInterval"))
    {
        g_vars.g_interval.poll_interval = json_int(root, "pollInterval");
    }
    if (cJSON_HasObjectItem(root, "pollIntervalCov"))
    {
        g_vars.g_interval.poll_interval_cov = json_int(root, "pollIntervalCov");
    }
    if (cJSON_HasObjectItem(root, "whoIsInterval"))
    {
        g_vars.g_interval.who_is_interval = json_int(root, "whoIsInterval");
    }
    if (cJSON_HasObjectItem(root, "heartbeatInterval"))
    {
        g_vars.g_interval.heart_beat_interval = json_int(root, "heartbeatInterval");
    }
    if (cJSON_HasObjectItem(root, "objectDiscoverInterval"))
    {
        g_vars.g_interval.object_discover_interval = json_int(root, "objectDiscoverInterval");
    }
    if (cJSON_HasObjectItem(root, "subscribeDuration"))
    {
        g_vars.g_interval.subscribe_duration = json_int(root, "subscribeDuration");
    }
    if (cJSON_HasObjectItem(root, "subscribeType"))
    {
        char* temp_str = json_string(root, "subscribeType");
        SUBSCRIBE_TYPE temp_subscribe_type = g_vars.g_interval.subscribe_type;
        if (strcmp(temp_str, "NoSubscribe") == 0) {
            temp_subscribe_type = NO_SUBSCRIBE;
        } else if (strcmp(temp_str, "SubscribeObject") == 0) {
            temp_subscribe_type = SUBSCRIBE_OBJECT;
        } else if (strcmp(temp_str, "SubscribeProperty") == 0) {
            temp_subscribe_type = SUBSCRIBE_PORPERTY;
        } else if (strcmp(temp_str, "SubscribePropertyWithCovIncrement") == 0) {
            temp_subscribe_type = SUBSCRIBE_PROPERTY_WITH_COV_INCREMENT;
        }

        if(g_vars.g_interval.subscribe_type == NO_SUBSCRIBE)
        {
            g_cancel_type = UN_NEED_TO_CANCEL;
        }
        else if(g_vars.g_interval.subscribe_type == SUBSCRIBE_OBJECT)
        {
            g_cancel_type = NEED_TO_CANCEL_OBJECT;
        }
        else
        {
            g_cancel_type = NEED_TO_CANCEL_PROPERTY;
        }

        if(g_vars.g_interval.subscribe_type != temp_subscribe_type)
        {
            g_vars.g_interval.subscribe_type = temp_subscribe_type;
            g_subscribe_type_change = 1;
        }
    }
    if (cJSON_HasObjectItem(root, "covIncrement"))
    {
        double temp_cov_increment = json_double(root, "covIncrement");
        if(g_vars.g_interval.subscribe_type != NO_SUBSCRIBE
            && fabs(temp_cov_increment - g_vars.g_interval.cov_increment) > 1e-6)
        {
            g_cov_increment_change = 1;
        }
        g_vars.g_interval.cov_increment = temp_cov_increment;
    }
    if (cJSON_HasObjectItem(root, "reset"))
    {
        if (json_bool(root, "reset"))
        {
            g_vars.g_reset_gateway = 1;
        }
    }
}

int msg_arrived(void* context, char* topicName, int topicLen, MQTTClient_message* message)
{
    // sometime we receive strange message with topic name like "\300\005@\267"
    // let's filter those message that with topic other than expected
    if (message == NULL)
    {
        return 1;
    }

    int i = 0;
    char* payloadptr = NULL;

    payloadptr = message->payload;
    int buflen = message->payloadlen + 1;
    char* buf = (char*)malloc(buflen*sizeof(char));
    buf[buflen - 1] = 0;
    for(i = 0; i < message->payloadlen; i++)
    {
        buf[i] = payloadptr[i];
    }

    MQTTClient_freeMessage(&message);

    if (is_string_valid_json(buf) == 0)
    {
        free(buf);
        MQTTClient_free(topicName);
        printf("received invalid json config:%s\n", buf);
        return 0;
    }

    cJSON* root = cJSON_Parse(buf);
    free(buf);
    if(root == NULL)
    {
        printf("the config string is not a valid json object, content=%s\n", buf);
        MQTTClient_free(topicName);
        return 0;
    }

    if (strcmp(g_vars.g_mqtt_info.sub_config, topicName) == 0)
    {
        // the parameters of interval
        log_debug("got message with the parameters of interval from \"config\" topic");
        // TODO
        msg_arrived_config(root);
    }
    else if (strcmp(g_vars.g_mqtt_info.sub_whois, topicName) == 0)
    {
        // sent who-is immediately
        log_debug("got message from \"whois\" topic and will be sent who-is immediately");
        // TODO
        send_whois_immediately();
    }
    else if (strcmp(g_vars.g_mqtt_info.sub_rpm, topicName) == 0)
    {
        // sent rpm immediately
        log_debug("got message from \"rpm\" topic and will be sent rpm immediately");
        // TODO
        send_rpm_immediately(root);
    }
    else if (strcmp(g_vars.g_mqtt_info.sub_wp, topicName) == 0)
    {
        log_debug("got message from \"wp\" topic and will be sent wpm immediately");
        send_wpm_immediately(root);
    }
    else
    {
        char buff[BUFF_LEN];
        snprintf(buff, BUFF_LEN,
                "received unrelevant message in command topic, skipping it. topic=%s, payload=%s"
                , topicName, payloadptr);
        log_debug(buff);
    }

    cJSON_Delete(root);
    MQTTClient_free(topicName);
    return 1;
}

void load_pull_policy(const char* file, Bac2mqttConfig* pconfig)
{
    printf("start to load data sampling policy from file:%s\n", file);

    // anyway we will clear the flag that need reload policy
    Thread_lock_mutex((g_vars.g_policy_update_lock));

    g_vars.g_policy_updated = 0;

    char* content = NULL;

    long filesize = read_file_as_string(file, &content);
    Thread_unlock_mutex((g_vars.g_policy_update_lock));

    if (filesize <= 0)
    {
        printf("failed to open policy cache file %s, skipping policy cache loading\n",
               file);

        return;
    }
    if (content == NULL)
    {
        return;
    }

    int rc = json2_bac2_mqtt_config(content, pconfig);
    if (rc == 0)
    {
        pconfig->rtConfLoaded = 1;
    }
}

void init_global_vars(GlobalVar* vars)
{
    vars->g_mqtt_info.last_heartbeat_time = 0;

    vars->g_gateway_connected = 0;
    vars->g_mqtt_client_mutex = Thread_create_mutex();
    vars->g_mqtt_client = NULL;

    vars->g_policy_lock = Thread_create_mutex();
    g_vars.g_policy_updated = 0;
    vars->g_policy_update_lock = Thread_create_mutex();

    vars->g_config.rtConfLoaded = 0;	// config not loaded yet
    vars->g_config.rtDeviceStarted = 0;	// this bacnet device not started yet
    vars->g_config.policyHeader.next = NULL;

    vars->g_all_devices.devices_num = 0;
    // device index from 1 to DEVICE_NUM_MAX
    vars->g_all_devices.devices_header = 
        (BacDevice2**) malloc((DEVICE_NUM_MAX + 1) * sizeof(BacDevice2*));

    vars->g_interval.poll_interval = 30;
    vars->g_interval.poll_interval_cov = 300;
    vars->g_interval.who_is_interval = 30;
    vars->g_interval.heart_beat_interval = 30;
    vars->g_interval.object_discover_interval = 3600;
    vars->g_interval.subscribe_duration = 600;

    vars->g_interval.subscribe_type = NO_SUBSCRIBE;
    vars->g_interval.cov_increment = 0.5;

    vars->g_reset_gateway = 0;

    set_global_vars(&g_vars);
}

void list_devices()
{
    BacDevice2** device = g_vars.g_all_devices.devices_header;
    printf("Instance\t\tObjectCount\r\n");
    int i = 0;
    for(i = 1; i <= g_vars.g_all_devices.devices_num; i++)
    {
        printf("%d\t\t%d\r\n", device[i]->instanceNumber, device[i]->objects_size);
    }
    printf("Total:%d\r\n", g_vars.g_all_devices.devices_num);
}

void list_devices_debug()
{
    BacDevice2** device = g_vars.g_all_devices.devices_header;
    printf("Instance\t\tObjectCount\t\tLastDiscoverTime\t\tReadObject\t\tDisabled\r\n");
    int i = 0;
    for(i = 1; i <= g_vars.g_all_devices.devices_num; i++)
    {
        printf("%d\t\t%d\t\t%ld\t\t%d\t\t%d\r\n", device[i]->instanceNumber, 
            device[i]->objects_size, 
            device[i]->last_discover_time, device[i]->update_objects, device[i]->disable);
    }
    printf("Total:%d\r\n", g_vars.g_all_devices.devices_num);
}

void list_objects(int instance) {
    uint32_t inst = (uint32_t) instance;
    BacDevice2** devices = g_vars.g_all_devices.devices_header;
    int i = 0;
    char found = 0;
    for(i = 1; i <= g_vars.g_all_devices.devices_num; i++)
    {
        if (inst == devices[i]->instanceNumber)
        {
            found = 1;
            printf("ObjectType\t\tObjectInstance\t\tCovEnabled\t\tLastReadTime\r\n");
            BacObject* object = devices[i]->objects_header;
            int cnt = 0;
            while(object)
            {
                printf("%s\t\t%d\t\t%d\t\t%ld\r\n", bactext_object_type_name(object->objectType),
                    object->objectInstance, 
                    object->support_cov, object->last_read_time);
                cnt++;
                object = object->next;
            }
            printf("Total:%d\r\n", cnt);
            break;
        }
    }
    if (found == 0) {
        printf("No device with instance number=%d\r\n", instance);
    }
}

void list_objects_debug(int instance) {
    uint32_t inst = (uint32_t) instance;
    BacDevice2** devices = g_vars.g_all_devices.devices_header;
    int i = 0;
    char found = 0;
    for(i = 1; i <= g_vars.g_all_devices.devices_num; i++)
    {
        if (inst == devices[i]->instanceNumber)
        {
            found = 1;
            printf("ObjectType\t\tObjectInstance\t\tCovEnabled\t\tLastReadTime\t\t");
            printf("LasSubscribeTime\t\tNeedRead\r\n");
            BacObject* object = devices[i]->objects_header;
            int cnt = 0;
            while(object)
            {
                printf("%s\t\t%d\t\t%d\t\t%ld\t\t%ld\t\t%d\r\n", 
                    bactext_object_type_name(object->objectType), object->objectInstance, 
                    object->support_cov, object->last_read_time, object->subscribe_time, 
                    object->need_to_read);
                cnt++;
                object = object->next;
            }
            printf("Total:%d\r\n", cnt);
            break;
        }
    }
    if (found == 0) {
        printf("No device with instance number=%d\r\n", instance);
    }
}

void wait_user_input()
{
    char cmd[MAX_LEN];
    char inst[MAX_LEN];
    int instance = -1;
    printf("Gateway is running, meanwhile, use following commands to control the gateway\r\n");
    printf("q: to exit\r\n");
    printf("d: to toggle debug info\r\n");
    printf("devices: to list bacnet devices found\r\n");
    printf("whois: to force gateway re-discover devices\r\n");
    printf("device <instance number>: to show found objects of a specify bacnet device\r\n");
    do
    {
        if (scanf("%s", cmd) <= 0) 
        {
            continue;
        }
        if (strcmp(cmd, "d") == 0 || strcmp(cmd, "d") == 0) 
        {
            toggle_debug();
        }

        if (strcmp(cmd, "q") == 0 || strcmp(cmd, "Q") == 0) 
        {
            break;
        }

        if (strcmp(cmd, "devices") == 0) 
        {
            list_devices();
        }

        if (strcmp(cmd, "devicesdebug") == 0) 
        {
            list_devices_debug();
        }

        if (strcmp(cmd, "device") == 0)
        {
            if (scanf("%s", inst) <= 0)
            {
                continue;
            }
            instance = atoi(inst);
            list_objects(instance);
        }

        if (strcmp(cmd, "devicedebug") == 0)
        {
            if (scanf("%s", inst) <= 0)
            {
                continue;
            }
            instance = atoi(inst);
            list_objects_debug(instance);
        }

        if (strcmp(cmd, "whois") == 0) 
        {
            send_whois_immediately();
        }

    } while(1);
    g_stop_worker = 1;
    printf("exiting...\n");
}

void insert_slave_policy(PullPolicy* policy)
{
    // insert a new slave policy into the list, ordered by the nextRun asc
    // so that the head of the list is always the next slave need to execute
    // note, this function could be involked in a different thread, make sure
    // add lock before modifying the list

    if (policy == NULL)
    {
        return;
    }

    // we already have the lock here.
    PullPolicy* itr = &g_vars.g_config.policyHeader;
    while (itr->next != NULL && itr->next->nextRun < policy->nextRun)
    {
        itr = itr->next;
    }
    policy->next = itr->next;
    itr->next = policy;
}

//TODO: fire up a few more workers, and precess in parallel, to speed up.
thread_return_type worker_func(void* arg)
{
    g_worker_is_running = 1;
    time_t last_time = time(NULL);
    time_t last_whois_time = last_time;
    time_t now_time = 0;
    while (g_stop_worker != 1)
    {
        if (g_vars.g_gateway_connected == 0)
        {
            log_debug("going to connect the mqtt client");
            start_mqtt_client(&g_vars, connection_lost, msg_arrived, delivered);
        }

        if (g_vars.g_gateway_connected == 0)
        {
            continue;
        }

        now_time = time(NULL);

        // mqtt heartbeat
        if(now_time - g_vars.g_mqtt_info.last_heartbeat_time
            >= g_vars.g_interval.heart_beat_interval)
        {
            mqtt_send_heartbeat(&g_vars);
            g_vars.g_mqtt_info.last_heartbeat_time = now_time;
        }

        if (g_vars.g_config.rtConfLoaded)
        {
            if(g_vars.g_config.rtDeviceStarted == 0)
            {
                start_local_bacnet_device(&g_vars.g_config);
                g_vars.g_config.rtDeviceStarted = 1;
            }

            // reset gateway
            if (g_vars.g_reset_gateway == 1) {
                reset_gateway();
                // update last_whois_time
                last_whois_time = now_time;

                g_vars.g_reset_gateway = 0;
            }

            if(now_time - last_whois_time >= g_vars.g_interval.who_is_interval)
            {
                send_whois_immediately();
                last_whois_time = now_time;
            }

            // iterate the devices
            if(g_vars.g_config.rtDeviceStarted == 1)
            {
                BacDevice2** device = g_vars.g_all_devices.devices_header;
                int i = 0;
                if((g_cov_increment_change == 1
                    && g_vars.g_interval.subscribe_type == SUBSCRIBE_PROPERTY_WITH_COV_INCREMENT)
                    || g_subscribe_type_change == 1)
                {
                    for(i = 1; i <= g_vars.g_all_devices.devices_num; i++)
                    {
                        if(device[i]->current_state != READ_OBJECT_LIST)
                        {
                            // re-subscribe or cancel subscribe
                            device[i]->handle_next_object = NULL;
                            device[i]->disable = 1;
                            device[i]->send_next = 0;
                            device[i]->last_ack_time = now_time;
                            BacObject* object = device[i]->objects_header;
                            device[i]->current_state = CANCEL_SUBSCRIBE_OBJECT;
                            while(object)
                            {
                                if(object->support_cov != UN_SUPPORT_SUB)
                                {
                                    if(g_subscribe_type_change)
                                    {
                                        if(g_cancel_type == NEED_TO_CANCEL_OBJECT)
                                        {
                                            object->support_cov = NEED_TO_CANCEL_OBJECT;
                                        }
                                        else if(g_cancel_type == NEED_TO_CANCEL_PROPERTY)
                                        {
                                            object->support_cov = NEED_TO_CANCEL_PROPERTY;
                                        }
                                        else
                                        {
                                            object->support_cov = NEED_TO_SUB;
                                        }
                                    }
                                    else
                                    {
                                        // only change cov increment
                                        // only re-subscribe ANALOG-* objects
                                        if(is_analog_object(object->objectType))
                                        {
                                            object->support_cov = NEED_TO_CANCEL_PROPERTY;
                                        }
                                    }
                                }
                                object = object->next;
                            }
                            if(g_vars.g_interval.subscribe_type == NO_SUBSCRIBE)
                            {
                                device[i]->current_state = CANCEL_SUBSCRIBE_OBJECT;
                            }
                            else
                            {
                                device[i]->current_state = CANCEL_AND_RE_SUBSCRIBE;
                            }
                            cancel_subscribe_object_if_need(device[i]);
                        }
                    }
                    g_cov_increment_change = 0;
                    g_subscribe_type_change = 0;
                    g_cancel_type = UN_NEED_TO_CANCEL;
                }

                for(i = 1; i <= g_vars.g_all_devices.devices_num; i++)
                {
                    if(now_time - device[i]->last_ack_time
                        >= g_vars.g_interval.poll_interval)
                    {
                        if(device[i]->current_state == ENTER_MAIN_LOOP)
                        {
                            if(device[i]->disable == 0)
                            {
                                // lost rpmack, reset
                                device[i]->send_next = 1;
                                device[i]->last_ack_time = now_time;
                                send_next_request(device[i]);
                            }
                        }
                        else
                        {
                            handle_for_unexpected(i);
                        }
                    }

                    if(device[i]->disable == 0)
                    {
                        if(now_time - device[i]->last_discover_time
                            >= g_vars.g_interval.object_discover_interval)
                        {
                            // read the struct of device objects and propertys
                            send_read_struct_list(device[i]);
                            device[i]->last_discover_time = now_time;
                        }
                        else
                        {
                            // iterate the object
                            BacObject* object = device[i]->objects_header;
                            while(object)
                            {
                                if(object->need_to_read == 0)
                                {
                                    if(object->support_cov == SUBSCRIBED
                                        && g_vars.g_interval.subscribe_type != NO_SUBSCRIBE)
                                    {
                                        if(now_time - object->subscribe_time
                                                >= g_vars.g_interval.subscribe_duration)
                                        { 
                                            // need to re-subscribe
                                            object->support_cov = NEED_TO_SUB;
                                            object->subscribe_time = now_time;
                                        }
                                        if(now_time - object->last_read_time
                                                >= g_vars.g_interval.poll_interval_cov)
                                        {
                                            // read object supported COV with a low frequency
                                            object->need_to_read = 1;
                                            object->last_read_time = now_time;
                                        }
                                    }
                                    else
                                    {
                                        if(now_time - object->last_read_time
                                                >= g_vars.g_interval.poll_interval)
                                        {
                                            // read object un-supported COV with a proper frequency
                                            object->need_to_read = 1;
                                            object->last_read_time = now_time;
                                        }
                                    }
                                }
                                object = object->next;
                            }
                            add_request_if_need(device[i]);
                            if(device[i]->send_next)
                            {
                                send_next_request(device[i]);
                            }
                        }
                    }
                }
            }
        }

        receive_and_handle();
        last_time = now_time;
        sleep_ms(5);
    }
    log_debug("exiting worker thread...\n");
    g_worker_is_running = 0;
    return NULL;
}

thread_type g_worker_thread;

void start_worker()
{
    g_worker_thread = Thread_start(worker_func, (void*) NULL);
}

void init_and_start()
{
    init_global_vars(&g_vars);

    load_mqtt_config(CONFIG_FILE, &(g_vars.g_mqtt_info));

    start_mqtt_client(&g_vars, connection_lost, msg_arrived, delivered);

    // lets sleep 1 second, in case any config sent with retain=true
    sleep_ms(1000);

    start_worker();
}

void free_char_pointer(char** pstr)
{
    if (pstr && *pstr)
    {
        free(*pstr);
        *pstr = NULL;
    }
}

void cleanup_device(char need_to_un_subscribe)
{
    // clean up all devices
    BacDevice2** device = g_vars.g_all_devices.devices_header;
    int i = 0;
    for(i = 1; i <= g_vars.g_all_devices.devices_num; i++)
    {
        free_device(device[i], need_to_un_subscribe);
    }
    device = NULL;
    g_vars.g_all_devices.devices_num = 0;
}

void cleanup_data()
{
    // clean up devices
    cleanup_device(1);
    free(g_vars.g_all_devices.devices_header);
    g_vars.g_all_devices.devices_header = NULL;
    
    // close the mqtt connection
    mqtt_cleanup(&g_vars);

    // clean up the mqtt info
    free_char_pointer(&g_vars.g_mqtt_info.endpoint);
    free_char_pointer(&g_vars.g_mqtt_info.pub_iam);
    free_char_pointer(&g_vars.g_mqtt_info.pub_rpmack);
    free_char_pointer(&g_vars.g_mqtt_info.pub_covnotice);
    free_char_pointer(&g_vars.g_mqtt_info.pub_heartbeat);
    free_char_pointer(&g_vars.g_mqtt_info.sub_topic);
    free_char_pointer(&g_vars.g_mqtt_info.sub_config);
    free_char_pointer(&g_vars.g_mqtt_info.sub_whois);
    free_char_pointer(&g_vars.g_mqtt_info.sub_rpm);
    free_char_pointer(&g_vars.g_mqtt_info.sub_wp);
    free_char_pointer(&g_vars.g_mqtt_info.user);
    free_char_pointer(&g_vars.g_mqtt_info.password);

    // clean up bacnet device info
    free_char_pointer(&g_vars.g_config.device.ipOrInterface);
}

void clean_and_exit()
{
    // pthread_join(g_worker_thread, NULL);
    int count = 0;
    while (g_worker_is_running == 1 && ++count < 10)
    {
        sleep_ms(1000);
    }
    cleanup_data();
}

void reset_gateway()
{
    printf("Gateway is being reset from cloud\n");
    // clean up all devices
    cleanup_device(0);
    // send who_is
    send_whois_immediately();
}