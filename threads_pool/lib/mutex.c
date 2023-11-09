#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>

#include "mutex.h"
#include "condvar.h"

typedef struct private_mutex_t private_mutex_t;
typedef struct private_condvar_t private_condvar_t;

struct private_condvar_t {
	condvar_t public;
	pthread_cond_t condvar;
};

struct private_mutex_t {
	mutex_t public;
	pthread_mutex_t mutex;
	bool recursive;
};

static void _lock(mutex_t *public)
{
    int err;
    private_mutex_t *this = (private_mutex_t *)public;

    err = pthread_mutex_lock(&this->mutex);
	if (err)
		fprintf(stderr, "!!! MUTEX LOCK ERROR: %s !!!", strerror(err));
}

static void _unlock(mutex_t *public)
{
    int err;
    private_mutex_t *this = (private_mutex_t *)public;

    err = pthread_mutex_unlock(&this->mutex);
	if (err)
		fprintf(stderr, "!!! MUTEX UNLOCK ERROR: %s !!!", strerror(err));
}

static void _destroy(mutex_t *public)
{
    private_mutex_t *this = (private_mutex_t *)public;
    pthread_mutex_destroy(&this->mutex);
    free(this);
}

mutex_t *mutex_create(mutex_type_t type)
{
    switch (type)
    {
        case MUTEX_TYPE_RECURSIVE:
            return NULL;
        case MUTEX_TYPE_DEFAULT:
		default:
		{
			private_mutex_t *this;

            this = malloc(sizeof(*this));
            memset(this, 0, sizeof(*this));

            this->public.lock = _lock;
            this->public.unlock = _unlock;
            this->public.destroy = _destroy;

			pthread_mutex_init(&this->mutex, NULL);

			return &this->public;
		}
    }
}

static void _wait_(condvar_t *pub_cond, mutex_t *pub_mutex)
{
    private_condvar_t *this = (private_condvar_t *)pub_cond;
    private_mutex_t *mutex = (private_mutex_t *)pub_mutex;
    pthread_cond_wait(&this->condvar, &mutex->mutex);
}

static bool _timed_wait_abs(condvar_t *pub_cond, mutex_t *pub_mutex, struct timeval tv)
{
    private_condvar_t *this = (private_condvar_t *)pub_cond;
    private_mutex_t *mutex = (private_mutex_t *)pub_mutex;
    bool timed_out;
    struct timespec ts;

    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = 1000 * tv.tv_usec;

    timed_out = pthread_cond_timedwait(&this->condvar, &mutex->mutex,
										   &ts) == ETIMEDOUT;
    return timed_out;
}

static bool _timed_wait(condvar_t *this, mutex_t *mutex, int timeout)
{
    struct timeval tv;
    int s, ms;

	gettimeofday(&tv, NULL);

	s = timeout / 1000;
	ms = timeout % 1000;

    tv.tv_sec += s;
    while (tv.tv_usec >= 1000000 /* 1s */)
	{
		tv.tv_usec -= 1000000;
		tv.tv_sec++;
	}
    return _timed_wait_abs(this, mutex, tv);
}

static void _signal_(condvar_t *public)
{
    private_condvar_t *this = (private_condvar_t *)public;
    pthread_cond_signal(&this->condvar);
}

static void _broadcast(condvar_t *public)
{
    private_condvar_t *this = (private_condvar_t *)public;
    pthread_cond_broadcast(&this->condvar);
}

static void _condvar_destroy(condvar_t *public)
{
    private_condvar_t *this = (private_condvar_t *)public;
    pthread_cond_destroy(&this->condvar);
	free(this);
}

condvar_t *condvar_create(condvar_type_t type)
{
    switch (type)
    {
        case CONDVAR_TYPE_DEFAULT:
		default:
		{
			private_condvar_t *this;

            this = malloc(sizeof(*this));
            memset(this, 0, sizeof(*this));
            this->public.wait = (void*)_wait_;
            this->public.timed_wait = (void*)_timed_wait;
            this->public.timed_wait_abs = (void*)_timed_wait_abs;
            this->public.signal = _signal_;
            this->public.broadcast = _broadcast;
            this->public.destroy = _condvar_destroy;
			
			{
				pthread_condattr_t condattr;
				pthread_condattr_init(&condattr);
				pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);
				pthread_cond_init(&this->condvar, &condattr);
				pthread_condattr_destroy(&condattr);
			}

			return &this->public;
		}
    }
}