// mqttsender.c

#include "mqttsender.h"
#include "ringbufi.h"
#include "thread.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <MQTTClient.h>

#define MAX_SENDER 64
#define WORKER_NOT_STARTED 0
#define WORKER_RUNNING 1
#define WORKER_REQUEST_STOP 2
#define WORKER_STOPPED 3
#define MAX_LEN 256

static MQTTClient_SSLOptions g_sslopts = MQTTClient_SSLOptions_initializer;

typedef struct MqttBrokerId_t
{
	char* endpoint;
	char* user;
	char* password;
	MQTTClient client;
	struct MqttBrokerId_t* next;
} MqttBrokerId;

typedef struct MqttMessageToPub_t
{
	char* endpoint;
	char* user;
	char* password;
	char* topic;
	char retain;
	char* payload;
	int payloadlen;
	char* certfile;
	struct MqttMessageToPub_t* next;
} MqttMessageToPub;

typedef struct 
{
	mutex_type lock;
	RingBuFi* ringbuf;
	MqttBrokerId* mqttClients;
	MqttBrokerId* badBrokers;
	volatile char status;
	thread_type worker;
	MqttMessageToPub incomingQueue;	// just the header, real msg start from next
} MqttSender;



static MqttSender* SENDERS[MAX_SENDER];
static char lock_initialized = 0;
static mutex_type sender_lock;// = Thread_create_mutex();

static size_t messageLen(const MqttMessageToPub* msg);
static void freeMsg(MqttMessageToPub* msg);
static size_t serializeMsg(const MqttMessageToPub* msg, void** output);
static MqttMessageToPub* deserializeMsg(const void* data);
static MQTTClient findExistingClient(const MqttSender* sender, const char* endpoint, const char* user);
static void freeBroker(MqttBrokerId* broker);
static void removeExistingClient(MqttSender* sender, MQTTClient client);
static char isKnownBadBroker(const MqttSender* sender, const char* endpoint, 
		const char* user, const char* password);
static void set_ssl_option(MQTTClient_connectOptions* conn_opts, const char* host, const char* pem);
static int makeMqttConnection(MQTTClient* client, const char* endpoint, const char* user,
		const char* password, const char* certfile);
static thread_return_type worker_func(void* arg);
static void byte_copy(void** dest, const void* src, int len, char padnull);

static void delivered(void* context, MQTTClient_deliveryToken dt)
{
    // printf("Message with token value %d delivery confirmed\n", dt);
}

static int msg_arrived(void* context, char* topicName, int topicLen, MQTTClient_message* message)
{
    printf("MqttSender msg_arrived topic=%s\n", topicName);
}


static void connection_lost(void* context, char* cause)
{
    printf("MqttSender Connection lost, caused by %s, will reconnect later\n", cause);
    // MQTTClient client = (MQTTClient) context;
    // MQTTClient_destroy(&client);
}

size_t messageLen(const MqttMessageToPub* msg)
{
	size_t len = 0;
	if (msg == NULL)
	{
		return len;
	}

	len += sizeof(size_t) + strlen(msg->endpoint);
	len += sizeof(size_t) + strlen(msg->user);
	len += sizeof(size_t) + strlen(msg->password);
	len += sizeof(size_t) + strlen(msg->topic);
	len += sizeof(char);
	len += sizeof(size_t) + msg->payloadlen;
	len += sizeof(size_t);
	if (msg->certfile != NULL)
	{
		len += strlen(msg->certfile);
	}

	return len;
}

void freeMsg(MqttMessageToPub* msg)
{
	if (msg != NULL)
	{
		if (msg->endpoint != NULL) 
		{
			free(msg->endpoint);
		}
		if (msg->user != NULL) 
		{
			free(msg->user);
		}
		if (msg->password != NULL) 
		{
			free(msg->password);
		}
		if (msg->topic != NULL) 
		{
			free(msg->topic);
		}
		if (msg->payload != NULL) 
		{
			free(msg->payload);
		}
		if (msg->certfile != NULL)
		{
			free(msg->certfile);
		}
		free(msg);

	}
}

size_t serializeMsg(const MqttMessageToPub* msg, void** output)
{
	size_t len = messageLen(msg);
	if (len <= 0) {
		return 0;
	}

	*output = malloc(len);
	size_t idx = 0;
	
	// endpoint
	size_t tempLen = strlen(msg->endpoint);
	memcpy(*output + idx, (void*) &tempLen, sizeof(size_t));
	idx += sizeof(size_t);
	memcpy(*output + idx, msg->endpoint, tempLen);
	idx += tempLen;

	// user
	tempLen = strlen(msg->user);
	memcpy(*output + idx, (void*) &tempLen, sizeof(size_t));
	idx += sizeof(size_t);
	memcpy(*output + idx, msg->user, tempLen);
	idx += tempLen;

	// password
	tempLen = strlen(msg->password);
	memcpy(*output + idx, (void*) &tempLen, sizeof(size_t));
	idx += sizeof(size_t);
	memcpy(*output + idx, msg->password, tempLen);
	idx += tempLen;

	// topic
	tempLen = strlen(msg->topic);
	memcpy(*output + idx, (void*) &tempLen, sizeof(size_t));
	idx += sizeof(size_t);
	memcpy(*output + idx, msg->topic, tempLen);
	idx += tempLen;

	// retain
	memcpy(*output + idx, (void*) &msg->retain, sizeof(char));
	idx += sizeof(char);

	// payload
	tempLen = msg->payloadlen;
	memcpy(*output + idx, (void*) &tempLen, sizeof(size_t));
	idx += sizeof(size_t);
	memcpy(*output + idx, msg->payload, tempLen);
	idx += tempLen;

	// certfile
	if (msg->certfile != NULL)
	{
		tempLen = strlen(msg->certfile);
		memcpy(*output + idx, (void*) &tempLen, sizeof(size_t));
		idx += sizeof(size_t);
		memcpy(*output + idx, msg->certfile, tempLen);
		idx += tempLen;
	}
	else
	{
		tempLen = 0;
		memcpy(*output + idx, (void*) &tempLen, sizeof(size_t));
		idx += sizeof(size_t);
	}
	return idx;
}

MqttMessageToPub* deserializeMsg(const void* data) 
{
	if (data == NULL)
	{
		return NULL;
	}

	MqttMessageToPub* msg = (MqttMessageToPub*) malloc(sizeof(MqttMessageToPub));
	msg->next = NULL;

	size_t idx = 0;
	size_t tempLen = 0;
	
	// endpoint
	memcpy((void*) &tempLen, data + idx, sizeof(size_t));
	idx += sizeof(size_t);
	byte_copy((void**)&msg->endpoint, data + idx, tempLen + 1, 1);
	idx += tempLen;

	// user
	memcpy((void*) &tempLen, data + idx, sizeof(size_t));
	idx += sizeof(size_t);
	byte_copy((void**)&msg->user, data + idx, tempLen + 1, 1);
	idx += tempLen;

	// password
	memcpy((void*) &tempLen, data + idx, sizeof(size_t));
	idx += sizeof(size_t);
	byte_copy((void**)&msg->password, data + idx, tempLen + 1, 1);
	idx += tempLen;

	// topic
	memcpy((void*) &tempLen, data + idx, sizeof(size_t));
	idx += sizeof(size_t);
	byte_copy((void**)&msg->topic, data + idx, tempLen + 1, 1);
	idx += tempLen;

	// retain
	msg->retain = * (char*) data + idx++;

	// payload
	memcpy((void*) &tempLen, data + idx, sizeof(size_t));
	msg->payloadlen = tempLen;
	idx += sizeof(size_t);
	byte_copy((void**)&msg->payload, data + idx, tempLen, 0);
	idx += tempLen;

	// certfile
	memcpy((void*) &tempLen, data + idx, sizeof(size_t));
	idx += sizeof(size_t);

	if (tempLen > 0)
	{
		byte_copy((void**)&msg->certfile, data + idx, tempLen + 1, 1);
		idx += tempLen;
	} 
	else 
	{
		msg->certfile = NULL;
	}

	return msg;
}



int new_mqtt_sender(const char* cacheFile, int cacheSize)
{
	if (lock_initialized == 0)
	{
		sender_lock = Thread_create_mutex();
		int j = 0; 
		for (; j < MAX_SENDER; j++) 
		{
			SENDERS[j] = NULL;
		}
		lock_initialized = 1;
	}
	RingBuFi* buf = newRingBuFi(cacheFile, (size_t) cacheSize);
	MqttSender* sender = (MqttSender*) malloc(sizeof(MqttSender));
	sender->lock = Thread_create_mutex();
	sender->ringbuf = buf;
	sender->mqttClients = NULL;
	sender->badBrokers = NULL;
	sender->status = WORKER_NOT_STARTED;
	sender->worker = Thread_start(worker_func, (void*) sender);
	sender->status = WORKER_RUNNING;
	sender->incomingQueue.next = NULL;
	Thread_lock_mutex(sender_lock);
	int i = 0; 
	int ret = -1;
	for (; i < MAX_SENDER; i++) 
	{
		if (SENDERS[i] == NULL)
		{
			SENDERS[i] = sender;
			ret = i;
			break;
		}
	}
	Thread_unlock_mutex(sender_lock);
	return ret;
}

void close_mqtt_sender(int handle)
{
	if (handle < 0 || handle >= MAX_SENDER) {
		return;
	}
	if (SENDERS[handle] != NULL)
	{
		MqttSender* sender = SENDERS[handle];
		Thread_lock_mutex(sender_lock);
		SENDERS[handle]->status = WORKER_REQUEST_STOP;
		closeRingBuFi(SENDERS[handle]->ringbuf);
		SENDERS[handle] = NULL;
		Thread_unlock_mutex(sender_lock);

		// wait for the worker thread to exit
		int cap = 10;
		while (cap-- > 0 && sender->status == WORKER_REQUEST_STOP)
		{
			sleep(1);
		}

		// close mqtt connections
		MqttBrokerId* broker = sender->mqttClients;
		while (broker != NULL)
		{
			MQTTClient_disconnect(broker->client, 3000);
			MqttBrokerId* next = broker->next;
			freeBroker(broker);
			broker = next;
		}
	}
}

MQTTClient findExistingClient(const MqttSender* sender, const char* endpoint, const char* user)
{
	if (sender == NULL || endpoint == NULL)
	{
		return NULL;
	}

	MqttBrokerId* broker = sender->mqttClients;
	while (broker != NULL)
	{
		if (strcmp(endpoint, broker->endpoint) == 0 && strcmp(user, broker->user) == 0)
		{
			return broker->client;
		}
		broker = broker->next;
	}

	return NULL;
}

void freeBroker(MqttBrokerId* broker)
{
	if (broker != NULL)
	{
		if (broker->endpoint != NULL)
		{
			free(broker->endpoint);
			broker->endpoint = NULL;
		}
		if (broker->user != NULL)
		{
			free(broker->user);
			broker->user = NULL;
		}
		if (broker->password != NULL)
		{
			free(broker->password);
			broker->password = NULL;
		}
		free(broker);
	}
}

void removeExistingClient(MqttSender* sender, MQTTClient client)
{
	if (sender == NULL)
	{
		return ;
	}

	MqttBrokerId* broker = sender->mqttClients;
	if (broker != NULL && broker->client == client)
	{
		sender->mqttClients = broker->next;
		return;
	}
	MqttBrokerId* pre = broker;
	broker = broker->next;
	while (broker != NULL)
	{
		if (broker->client == client)
		{
			pre->next = broker->next;
			broker->next = NULL;
			freeBroker(broker);
			break;
		}
		pre = broker;
		broker = broker->next;
	}

}


char isKnownBadBroker(const MqttSender* sender, const char* endpoint, 
		const char* user, const char* password)
{
	if (sender == NULL || endpoint == NULL)
	{
		return 0;
	}

	MqttBrokerId* broker = sender->badBrokers;
	while (broker != NULL)
	{
		if (strcmp(endpoint, broker->endpoint) == 0 
			&& strcmp(user, broker->user) == 0
			&& strcmp(password, broker->password))
		{
			return 1;
		}
		broker = broker->next;
	}

	return 0;
}

void set_ssl_option(MQTTClient_connectOptions* conn_opts, const char* host, const char* pem)
{
    char* ssl = "ssl://";
    char* ssl_upper = "SSL://";
    if (strncmp(ssl, host, strlen(ssl)) == 0
        || strncmp(ssl_upper, host, strlen(ssl_upper)) == 0)
    {
        g_sslopts.trustStore = pem;
        g_sslopts.enableServerCertAuth = 1;
        conn_opts->ssl = &g_sslopts;
    }
}

int makeMqttConnection(MQTTClient* client, const char* endpoint, const char* user,
		const char* password, const char* certfile)
{

	char clientid[256];
    snprintf(clientid, MAX_LEN, "MqttSender%lld", (long long)time(NULL));
	int rc = MQTTClient_create(client, endpoint,
             clientid, MQTTCLIENT_PERSISTENCE_NONE, NULL);
	MQTTClient_connectOptions connect_options = MQTTClient_connectOptions_initializer;
    connect_options.keepAliveInterval = 20;  // Alive interval
    connect_options.cleansession = 1;
    connect_options.username = user;
    connect_options.password = password;
    connect_options.connectTimeout = 5;
    set_ssl_option(&connect_options, endpoint, certfile);
	MQTTClient_setCallbacks(*client, *client, connection_lost, msg_arrived, delivered);
    rc = MQTTClient_connect(*client, &connect_options);
    return rc;
}

static flushIncomingQueueToFile(MqttSender* sender)
{
	if (sender != NULL && sender->incomingQueue.next != NULL)
	{
		Thread_lock_mutex(sender->lock);
		MqttMessageToPub* tosave = sender->incomingQueue.next;
		sender->incomingQueue.next = NULL;
		Thread_unlock_mutex(sender->lock);
		while (tosave != NULL)
		{
			void* bytes;
			size_t msg_len = serializeMsg(tosave, &bytes);
			if (bytes != NULL && msg_len > 0) 
			{
				putRingBuFiRecord(sender->ringbuf, bytes, msg_len);
				free(bytes);
			}
			MqttMessageToPub* todel = tosave;
			tosave = tosave->next;
			freeMsg(todel);
		}
	}
}
static thread_return_type worker_func(void* arg)
{
	// peek a record if any
	// establish mqtt connection if need
	// send the record and remove from buf
	MqttSender* sender = (MqttSender*) arg;
	MqttMessageToPub sendingQueue;
	sendingQueue.next = NULL;
	MqttMessageToPub* msg = NULL;
	while (sender != NULL && sender->status != WORKER_REQUEST_STOP)
	{
		if (sendingQueue.next != NULL)
		{
			// 1, save incoming queue into file
			flushIncomingQueueToFile(sender);
		}
		else
		{
			// if buffer is not empty, save incoming queue to buffer 
			// and fetch data from buffer
			// otherwise try to get from incoming queue
			if (! isRingBuFiEmpty(sender->ringbuf))
			{
				flushIncomingQueueToFile(sender);

				// fetch one record from buffer
				void* data = NULL;
				size_t len = 0;
				peekRingBuFiRecord(sender->ringbuf, &data, &len);
				popRingBuFiRecord(sender->ringbuf);
				if (data != NULL && len > 0)
				{
					sendingQueue.next = deserializeMsg(data);
					free(data);
				}
			}
			else
			{
				// get data from incoming queue directory
				if (sender->incomingQueue.next != NULL)
				{
					Thread_lock_mutex(sender->lock);
					sendingQueue.next = sender->incomingQueue.next;
					sender->incomingQueue.next = NULL;
					Thread_unlock_mutex(sender->lock);
				}
			}
		}

		if (sendingQueue.next == NULL)
		{
			// no date to send, sleep for a while
			sleep(1);
		}
		else
		{
			msg = sendingQueue.next;
			// it's known bad broker?
			if (isKnownBadBroker(sender, msg->endpoint, msg->user, msg->password))
			{
				// discord this message and continue
				sendingQueue.next = sendingQueue.next->next;
				freeMsg(msg);
				printf("got msg of an known bad broker\n");
				continue;
			}

			MQTTClient mqttClient = findExistingClient(sender, msg->endpoint, msg->user);
			if (mqttClient == NULL)
			{
				// let's make a connection
				MQTTClient newClient;
				int rc = makeMqttConnection(&newClient, msg->endpoint, msg->user, msg->password,
				 msg->certfile);

				if (rc != MQTTCLIENT_SUCCESS)
				{
					MQTTClient_destroy(&newClient);
					if (rc == 1 	// Unacceptable protocol version
						|| rc == 4	// Bad user name or password
						|| rc == 5)	// Not authorized
					{
						MqttBrokerId* badBroker = (MqttBrokerId*) malloc(sizeof(MqttBrokerId));
						byte_copy((void**)&badBroker->endpoint, msg->endpoint, strlen(msg->endpoint) + 1, 1);
						byte_copy((void**)&badBroker->user, msg->user, strlen(msg->user) + 1, 1);
						byte_copy((void**)&badBroker->password, msg->password, strlen(msg->password) + 1, 1);
						badBroker->next = sender->badBrokers;
						sender->badBrokers = badBroker;
						printf("Found a bad broker\n");
					}
					sleep(1);
					continue;
				}

				mqttClient = newClient;
				MqttBrokerId* broker = (MqttBrokerId *) malloc(sizeof(MqttBrokerId));
				byte_copy((void**)&broker->endpoint, msg->endpoint, strlen(msg->endpoint) + 1, 1);
				byte_copy((void**)&broker->user, msg->user, strlen(msg->user) + 1, 1);
				byte_copy((void**)&broker->password, msg->password, strlen(msg->password) + 1, 1);
				
				broker->client = mqttClient;
				broker->next = sender->mqttClients;
				sender->mqttClients = broker;
			}

			MQTTClient_message pubmsg = MQTTClient_message_initializer;
	        MQTTClient_deliveryToken delivery_token;
	        
	        pubmsg.payload = msg->payload;
	        pubmsg.payloadlen = msg->payloadlen;
	        pubmsg.qos = 1;
	        pubmsg.retained = msg->retain;

	        int rc = MQTTClient_publishMessage(mqttClient,
	                 msg->topic, &pubmsg, &delivery_token);
	        
	        if (rc == MQTTCLIENT_SUCCESS)
	        {
	        	int waitrc = MQTTClient_waitForCompletion(mqttClient, delivery_token, 10000L);
	        	if (waitrc == MQTTCLIENT_SUCCESS)
	        	{
	        		sendingQueue.next = sendingQueue.next->next;
					freeMsg(msg);
				}
	        }
	        else
	        {
	        	removeExistingClient(sender, mqttClient);
	        	MQTTClient_disconnect(mqttClient, 3000);
	        	MQTTClient_destroy(&mqttClient);
	        	sleep(1);
	        }
	    }
	    					
	}
	sender->status = WORKER_STOPPED;
}

char mqtt_send(int handle, 
	const char* endpoint, 
	const char* username,
	const char* password,
	const char* topic,
	const char* payload,
	int payloadlen,
	char retain,
	const char* certfile)
{
	// 0, check if handle is valid or not
	if (handle < 0 || handle >= MAX_SENDER || SENDERS[handle] == NULL)
	{
		return -1;
	}

	// 1, check if it's a bad broker
	if (isKnownBadBroker(SENDERS[handle], endpoint, username, password) == 1) {
		return -1;
	}

	// add to ring buffer
	MqttMessageToPub* msg = (MqttMessageToPub*) malloc(sizeof(MqttMessageToPub));
	msg->next = NULL;
	int tempLen = strlen(endpoint);
	byte_copy((void**)&msg->endpoint, endpoint, tempLen + 1, 1);

	tempLen = strlen(username);
	byte_copy((void**)&msg->user, username, tempLen + 1, 1);

	tempLen = strlen(password);
	byte_copy((void**)&msg->password, password, tempLen + 1, 1);

	tempLen = strlen(topic);
	byte_copy((void**)&msg->topic, topic, tempLen + 1, 1);

	tempLen = payloadlen;
	byte_copy((void**)&msg->payload, payload, tempLen, 0);
	msg->payloadlen = payloadlen;

	msg->retain = retain;

	if (certfile == NULL) {
		msg->certfile = NULL;
	} 
	else 
	{
		tempLen = strlen(certfile);
		byte_copy((void**)&msg->certfile, certfile, tempLen + 1, 1);
	}


	MqttSender* sender = SENDERS[handle];
	Thread_lock_mutex(sender->lock);
	MqttMessageToPub* list = &(sender->incomingQueue);
	while (list->next != NULL)
	{
		list = list->next;
	}
	list->next = msg;
	Thread_unlock_mutex(sender->lock);
}

void byte_copy(void** dest, const void* src, int len, char padnull)
{
	if (src == NULL || len <= 0) {
		*dest = NULL;
		return;
	}

	*dest = malloc(len);
	
	if (padnull == 1)
	{
		memcpy(*dest, src, len - 1);
		char* str = (char*) *dest;
		str[len - 1] = '\0';
	}
	else
	{
		memcpy(*dest, src, len);
	}
}

