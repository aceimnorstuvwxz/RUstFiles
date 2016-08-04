#include <unistd.h>



void payload_init(void)
{
    printf(__FUNCTION__);
}

void payload_close(void)
{
    printf(__FUNCTION__);
}

int payload_run(char * seq)
{
    printf("payload_run  %s", seq);

    sleep(2);

    return 0;
}