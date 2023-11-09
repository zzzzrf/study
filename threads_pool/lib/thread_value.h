#ifndef __MY_THREAD_VALUE_H__
#define __MY_THREAD_VALUE_H__

#include "thread.h"

typedef struct thread_value_t thread_value_t;

/**
 * Wrapper for thread-specific values.
 */
struct thread_value_t {

	/**
	 * Set a thread-specific value.
	 *
	 * @param val		thread specific value
	 */
	void (*set)(thread_value_t *this, void *val);

	/**
	 * Get a thread-specific value.
	 *
	 * @return			the value specific to the current thread
	 */
	void *(*get)(thread_value_t *this);

	/**
	 * Destroys this thread specific value wrapper. There is no check for
	 * non-NULL values which are currently assigned to the calling thread, no
	 * destructor is called.
	 */
	void (*destroy)(thread_value_t *this);

};

/**
 * Create a new thread-specific value wrapper.
 *
 * The optional destructor is called whenever a thread terminates, with the
 * assigned value as argument. It is not called if that value is NULL.
 *
 * @param destructor	destructor
 * @return				thread-specific value wrapper
 */
thread_value_t *thread_value_create(thread_cleanup_t destructor);

#endif