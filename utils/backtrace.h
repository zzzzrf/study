#ifndef __MY_BACKTRACE_H__
#define __MY_BACKTRACE_H__

#include <stdio.h>
#include <stdbool.h>

#define BT_BUF_SIZE 100

typedef struct backtrace_t backtrace_t;

struct backtrace_t
{
    void (*log)(backtrace_t *this, FILE *file, bool detailed);
    void (*destroy)(backtrace_t *this);
};

backtrace_t *backtrace_create(int skip);

void backtrace_dump(char *label, FILE *file, bool detailed);

void backtrace_init();
void backtrace_deinit();
#endif