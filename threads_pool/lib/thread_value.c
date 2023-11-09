#include <stdlib.h>
#include <pthread.h>

#include "thread_value.h"

typedef struct private_thread_value_t private_thread_value_t;

struct private_thread_value_t {
	/**
	 * Public interface.
	 */
	thread_value_t public;

	/**
	 * Key to access thread-specific values.
	 */
	pthread_key_t key;

	/**
	 * Destructor to cleanup the value of the thread destroying this object
	 */
	thread_cleanup_t destructor;

};


static void _set(thread_value_t *public, void *val)
{
    private_thread_value_t *this = (private_thread_value_t *)public;
    pthread_setspecific(this->key, val);
}

static void *_get(thread_value_t *public)
{
    private_thread_value_t *this = (private_thread_value_t *)public;
    return pthread_getspecific(this->key);
}

static void _destroy(thread_value_t *public)
{  
    void *val;
    private_thread_value_t *this = (private_thread_value_t *)public;

    /* the destructor is not called automatically for the thread calling
	 * pthread_key_delete() */
    if (this->destructor)
	{
		val = pthread_getspecific(this->key);
		if (val)
		{
			this->destructor(val);
		}
	}
	pthread_key_delete(this->key);
	free(this);
}

/**
 * Described in header.
 */
thread_value_t *thread_value_create(thread_cleanup_t destructor)
{
	private_thread_value_t *this;

    this = calloc(1, sizeof(*this));

    this->destructor = destructor;

    this->public.set = _set;
    this->public.get = _get;
    this->public.destroy = _destroy;

	pthread_key_create(&this->key, destructor);
	return &this->public;
}

