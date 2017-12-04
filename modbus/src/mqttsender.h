
#ifndef MQTT_SENDER_H
#define MQTT_SENDER_H

// create a new mqtt sender handle; cacheFile will be 
// created if not exist, otherwise will read existing
// cache data from the file; return -1 on failure
int new_mqtt_sender(const char* cacheFile, int cacheSize);

// close the mqtt sender
void close_mqtt_sender(int handle);


// try to send a mqtt message
// data will be cached if mqtt connection
// is disconnected. order will be reserved
// return 0 on success, -1 on error
char mqtt_send(int handle, 
	const char* endpoint, 
	const char* username,
	const char* password,
	const char* topic,
	const char* payload,
	int payloadlen,
	char retain,
	const char* certfile); 


#endif



