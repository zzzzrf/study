#include "backtrace.h"
#include <stdio.h>

void terrible_function()
{
    printf("%s\n", __LINE__);
}