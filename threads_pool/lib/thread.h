#ifndef __MY_THREAD_H__
#define __MY_THREAD_H__

#include <stdbool.h>

typedef struct thread_t thread_t;
typedef void *(*thread_main_t)(void *arg);
typedef void (*thread_cleanup_t)(void *arg);

struct thread_t {
	void (*cancel)(thread_t *this);
	void (*kill)(thread_t *this, int sig);
	void (*detach)(thread_t *this);
	void *(*join)(thread_t *this);
};

void threads_init();
void threads_deinit();

bool thread_cancelability(bool enable);
int thread_current_id();

void thread_cleanup_push(thread_cleanup_t cleanup, void *arg);
void thread_cleanup_pop(bool execute);

thread_t *thread_current();

thread_t *thread_create(thread_main_t main, void *arg);
#endif