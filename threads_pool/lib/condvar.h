#ifndef __MY_CONDVAR_T__
#define __MY_CONDVAR_T__

typedef struct condvar_t condvar_t;
typedef enum condvar_type_t condvar_type_t;

#include <stdbool.h>
#include <sys/time.h>
#include "mutex.h"

enum condvar_type_t {
	CONDVAR_TYPE_DEFAULT = 0,
};

struct condvar_t {

	void (*wait)(condvar_t *this, mutex_t *mutex);

	bool (*timed_wait)(condvar_t *this, mutex_t *mutex, int timeout);

	bool (*timed_wait_abs)(condvar_t *this, mutex_t *mutex, struct timeval tv);

	void (*signal)(condvar_t *this);

	void (*broadcast)(condvar_t *this);

	void (*destroy)(condvar_t *this);
};

condvar_t *condvar_create(condvar_type_t type);

#endif