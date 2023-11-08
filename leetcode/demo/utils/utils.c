#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "utils.h"

int *gen_random_arr(int maxsize, int maxnum, int *len)
{
    int size = random() % maxsize;
    int *ret = malloc(sizeof(int) * size);
    *len = size;

    for (int i = 0; i < size; i++)
        ret[i] = random() % maxnum - random() % maxnum;

    return ret;
}