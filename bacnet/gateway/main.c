#include "business.h"
#include <stdio.h>

const int VER = 1;
int main(int argc, char *argv[])
{
    printf("Baidu IoT BACnet Gateway, build %d\r\n", VER);
    init_and_start();

    wait_user_input();

    clean_and_exit();

    return 0;
}
