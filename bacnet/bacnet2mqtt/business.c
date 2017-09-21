
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "jsonutil.h"
#include "mqttutil.h"
#include "baclib.h"


const char* const CONFIG_FILE = "gwconfig-bacnet.txt";
const char* const POLICY_CACHE = "policyCache-bacnet.txt";

GlobalVar g_vars;
char g_buff[BUFF_LEN];
int g_stop_worker = 0;
int g_worker_is_running = 0;

void load_mqtt_config(const char* file, MqttInfo* pInfo) {
	printf("starting to load gateway mqtt config from file %s\n", CONFIG_FILE);

	char* content = NULL;
    long size = read_file_as_string(file, &content);
    if (size <= 0)
    {
        printf("failed to open config file %s, while trying to load gateway configuration\n",
             file);
        exit(1);
    }

	json2MqttInfo(content, pInfo);

}

void connection_lost(void* context, char* cause)
{
    printf("\nConnection lost, caused by %s, will reconnect later\n", cause);
    Thread_lock_mutex((g_vars.g_gateway_mutex));
    g_vars.g_gateway_connected = 0;
    Thread_unlock_mutex((g_vars.g_gateway_mutex));
}

void delivered(void* context, MQTTClient_deliveryToken dt)
{
    printf("Message with token value %d delivery confirmed\n", dt);
}

int msg_arrived(void* context, char* topicName, int topicLen, MQTTClient_message* message)
{
    // sometime we receive strange message with topic name like "\300\005@\267"
    // let's filter those message that with topic other than expected
    if (message == NULL)
    {
        return 1;
    }
    if (strcmp(g_vars.g_mqtt_info.configTopic, topicName) != 0)
    {
    	if (strcmp(g_vars.g_mqtt_info.controlTopic, topicName) == 0) {
    		printf("WARN:got message from control topic:%s, but control is not implemented\n", topicName);
    		return 1;
    	} else {
	    	char buff[BUFF_LEN];
	        snprintf(buff, BUFF_LEN, 
	                "received unrelevant message in command topic, skipping it. topic=%s, payload=", 
	                topicName);
	        log_debug(buff);
	    
	        MQTTClient_freeMessage(&message);
	        MQTTClient_free(topicName);

	        return 1;
    	}
    }

    int i = 0;
    char* payloadptr = NULL;

    payloadptr = message->payload;
    int buflen = message->payloadlen + 1;
    char* buf = (char*)malloc(buflen);
    buf[buflen - 1] = 0;
    for(i = 0; i<message->payloadlen; i++)
    {
        buf[i] = payloadptr[i];
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

    if (isStringValidJson(buf) == 0)
    {
        free(buf);
        printf("received invalid json config:%s\n", buf);
        return 1;
    }

    Thread_lock_mutex((g_vars.g_policy_update_lock));
    FILE* fp = fopen(POLICY_CACHE, "w");
    if (! fp)
    {
        free(buf);
        snprintf(g_buff, BUFF_LEN, "failed to open %s for write", POLICY_CACHE);
        log_debug(g_buff);
        Thread_unlock_mutex((g_vars.g_policy_update_lock));
        return 0;
    }
    fprintf(fp, "%s", buf);
    fclose(fp);
    printf("received following config:\n%s\n", buf);
    free(buf);

    g_vars.g_policy_updated = 1;
    Thread_unlock_mutex((g_vars.g_policy_update_lock));
    return 1;
}

void load_pull_policy(const char* file, Bac2mqttConfig* pconfig) {
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

    int rc = json2Bac2mqttConfig(content, pconfig);
    if (rc == 0) {
    	pconfig->rtConfLoaded = 1;
    }
}

void init_global_vars(GlobalVar* vars) {

	vars->g_gateway_connected = 0;
	vars->g_mqtt_client_mutex = Thread_create_mutex();

	vars->g_policy_lock = Thread_create_mutex();
	g_vars.g_policy_updated = 0;
	vars->g_policy_update_lock = Thread_create_mutex();

	vars->g_config.rtConfLoaded = 0;	// config not loaded yet
	vars->g_config.rtDeviceStarted = 0;	// this bacnet device not started yet
	vars->g_config.policyHeader.next = NULL;

	set_global_vars(&g_vars);
}

void wait_user_input()
{
    char ch = '\0';
    printf("Gateway is running, press 'q' to exit, press 'd' to toggle debug\n");
    do 
    {
        ch = getchar();
        
        // toggle debug on/off
        if (ch == 'd' || ch == 'D')
        {
            toggle_debug();
        }
    } while(ch!='Q' && ch != 'q'); 
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


void execute_policy(PullPolicy* policy)
{
	if (policy == NULL) {
		return;
	}
    // 1 recaculate the next run time
    policy->nextRun = policy->interval + time(NULL);

    // 2 re-insert into the list, in order of nextRun
    insert_slave_policy(policy);

    // 3 issue the property read request
    issue_read_property_multiple(policy);
}

//TODO: fire up a few more workers, and precess in parallel, to speed up.
thread_return_type worker_func(void* arg)
{
    g_worker_is_running = 1;	
    while (g_stop_worker != 1)
    {
        // load slave policy if it's updated
        if (g_vars.g_policy_updated)
        {
            load_pull_policy(POLICY_CACHE, &(g_vars.g_config));
        }
        
        if (g_vars.g_gateway_connected == 0)
        {
            log_debug("going to connect the mqtt client");
            start_mqtt_client(&g_vars, connection_lost, msg_arrived, delivered);
        }

        // iterate from the beginning of the policy list
        // and pick those whose nextRun is due, and execute them, 
        // calculate the new next run, and insert into the list
        if (g_vars.g_config.rtConfLoaded) {
        	if (g_vars.g_config.rtDeviceStarted == 0) {
        		start_local_bacnet_device(&g_vars.g_config);
        		g_vars.g_config.rtDeviceStarted = 1;
        	}
        	//printf("rtDeviceStarted=%d\n", g_vars.g_config.rtDeviceStarted);
        	if (g_vars.g_config.rtDeviceStarted == 1) {
        		bind_bac_device_address(&g_vars.g_config);
        	}
        	if (g_vars.g_config.rtDeviceStarted == 1) {
		        time_t now = time(NULL);
		        // we have something to do, acquire the lock here
		        Bac2mqttConfig* theConfig = &g_vars.g_config;
		        Thread_lock_mutex(g_vars.g_policy_lock);
		        if (theConfig->policyHeader.next != NULL && theConfig->policyHeader.next->nextRun <= now)
		        {    
		            while (theConfig->policyHeader.next != NULL && theConfig->policyHeader.next->nextRun <= now)
		            {
		                // detach from the list
		                PullPolicy* policy = theConfig->policyHeader.next;
		                theConfig->policyHeader.next = theConfig->policyHeader.next->next;
		                execute_policy(policy);
		            }
		        }
		        Thread_unlock_mutex(g_vars.g_policy_lock);   
	    	}
        } 

        sleep_ms(300);
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

void init_and_start() {
	init_global_vars(&g_vars);

	load_mqtt_config(CONFIG_FILE, &(g_vars.g_mqtt_info));

	start_mqtt_client(&g_vars, connection_lost, msg_arrived, delivered);

	// lets sleep 1 second, in case any config sent with retain=true
	sleep_ms(500);

	load_pull_policy(POLICY_CACHE, &(g_vars.g_config));

	start_worker();
	//worker_func(NULL);
}

void freeCharPointer(char** pstr) {
	if (pstr && *pstr) {
		free(*pstr);
		*pstr = NULL;
	}
}
void cleanup_data() {
	// close the mqtt connection
	mqtt_cleanup(&g_vars);

	// clean up the mqtt info
	freeCharPointer(&g_vars.g_mqtt_info.endpoint);
	freeCharPointer(&g_vars.g_mqtt_info.configTopic);
	freeCharPointer(&g_vars.g_mqtt_info.dataTopic);
	freeCharPointer(&g_vars.g_mqtt_info.controlTopic);
	freeCharPointer(&g_vars.g_mqtt_info.user);
	freeCharPointer(&g_vars.g_mqtt_info.password);
	
	// clean up pull policies
	PullPolicy* pPolicy = g_vars.g_config.policyHeader.next;
	int i = 0;
	while (pPolicy) {
		for (i = 0; i < pPolicy->propNum; i++) {
			if (pPolicy->properties[i] != NULL) {
				free(pPolicy->properties[i]);
				pPolicy->properties[i] = NULL;
			}
		}
		pPolicy->propNum = 0;
		PullPolicy* tmp = pPolicy;
		pPolicy = pPolicy->next;
		free(tmp);
	}
	g_vars.g_config.policyHeader.next = NULL;

	// clean up bacnet device info
	freeCharPointer(&g_vars.g_config.device.ipOrInterface);
}

void clean_and_exit()
{
    // pthread_join(g_worker_thread, NULL);
    int count = 0;
    while (g_worker_is_running == 1 && ++count < 10) {
        sleep(1);
    }
    cleanup_data();
}

