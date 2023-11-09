#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "thread.h"

static void cleanup1(void *arg)
{
    printf("[%s]\n", __func__);
}

static void cleanup2(void *arg)
{
    printf("[%s]\n", __func__);
}

static void *start(void *arg)
{
    thread_cleanup_push(cleanup1, NULL);
    thread_cleanup_push(cleanup2, NULL);
    printf("%p\n", arg);
    sleep(1);
    printf("[%s] end\n", __func__);
    return NULL;
}

int main(void)
{
    threads_init();
    thread_t *th = thread_create(start, NULL);
    th->join(th);
    threads_deinit();
    return 0;
}