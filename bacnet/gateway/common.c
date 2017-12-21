#include "common.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#ifdef WIN32
#include <windows.h>
#elif _POSIX_C_SOURCE >= 199309L
#include <time.h>   // for nanosleep
#else
#include <unistd.h> // for usleep
#endif

void sleep_ms(int milliseconds) // cross-platform sleep function
{
#ifdef WIN32
    Sleep(milliseconds);
#elif _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
#else
    usleep(milliseconds * 1000);
#endif
}

// common function section
long read_file_as_string(char const* path, char** buf)
{
    FILE* fp = NULL;
    size_t fsz = 0;
    long   off_end = 0;
    int    rc = 0;

    fp = fopen(path, "rb");
    if(NULL == fp)
    {
        return -1L;
    }

    rc = fseek(fp, 0L, SEEK_END);
    if(0 != rc)
    {
        return -1L;
    }

    if(0 > (off_end = ftell(fp)))
    {
        return -1L;
    }
    fsz = (size_t)off_end;

    *buf = malloc(fsz + 1);
    if(NULL == *buf)
    {
        return -1L;
    }

    rewind(fp);

    if(fsz != fread(*buf, 1, fsz, fp))
    {
        free(*buf);
        return -1L;
    }

    if(EOF == fclose(fp))
    {
        free(*buf);
        return -1L;
    }

    return (long)fsz;
}

int g_debug = 0;
void toggle_debug()
{
    g_debug = g_debug == 0 ? 1 : 0;
    if (g_debug == 1)
    {
        printf("debug info is on\n");
    }
    else
    {
        printf("debug info is off\n");
    }
}

void log_debug(char* msg)
{
    if (g_debug == 1)
    {
        time_t now = time(NULL);
        printf("%s %s\n", ctime(&now), msg);
    }
}

void mystrncpy(char* desc, const char* src, int len)
{
    snprintf(desc, len, "%s", src);
}

int json_int(cJSON* root, char* item)
{
    return cJSON_GetObjectItem(root, item)->valueint;
}

double json_double(cJSON* root, char* item)
{
    return cJSON_GetObjectItem(root, item)->valuedouble;
}

char* json_string(cJSON* root, char* item)
{
    return cJSON_GetObjectItem(root, item)->valuestring;
}

int json_bool(cJSON* root, char* item)
{
    cJSON* b = cJSON_GetObjectItem(root, item);
    if(cJSON_IsTrue(b))
    {
        return 1;
    }
    return 0;
}

char* create_topic(const char* topic_prefix, const char* topic_suffix)
{
    int len_prefix = strlen(topic_prefix);
    int len_suffix = strlen(topic_suffix);
    if(len_suffix == 0)
    {
        int len = len_prefix + 1;
        char* topic = (char*) malloc(len*sizeof(char));
        snprintf(topic, len, "%s", topic_prefix);
        return topic;
    }
    int len = len_prefix + len_suffix + 2;
    char* topic = (char*) malloc(len*sizeof(char));

    snprintf(topic, len, "%s/%s", topic_prefix, topic_suffix);
    return topic;
}