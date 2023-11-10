#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "thread.h"
#include "processor.h"

int main(void)
{
    threads_init();
    
    processor_t *processor = processor_create();
    processor->set_threads(processor, 1);

    pause();
    threads_deinit();
    return 0;
}