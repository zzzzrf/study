#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/queue.h>

#include "thread.h"
#include "mutex.h"
#include "thread_value.h"

#include <sys/types.h>
#include <unistd.h>

typedef struct private_thread_t private_thread_t;


typedef struct {
	/**
	 * Cleanup callback function.
	 */
	thread_cleanup_t cleanup;

	/**
	 * Argument provided to the cleanup function.
	 */
	void *arg;

} cleanup_handler_t;

struct thread_clean_entry
{
    TAILQ_ENTRY(thread_clean_entry) entries;
    cleanup_handler_t handler;
};

TAILQ_HEAD(thread_clean_list, thread_clean_entry);

struct private_thread_t {
	/**
	 * Public interface.
	 */
	thread_t public;

	/**
	 * Identifier of this thread (human-readable/thread ID).
	 */
	int id;

	/**
	 * ID of the underlying thread.
	 */
	pthread_t thread_id;

	/**
	 * Main function of this thread (NULL for the main thread).
	 */
	thread_main_t main;

	/**
	 * Argument for the main function.
	 */
	void *arg;

	/**
	 * Stack of cleanup handlers.
	 */
	struct thread_clean_list cleanup_handlers;

	/**
	 * Mutex to make modifying thread properties safe.
	 */
	mutex_t *mutex;

	/**
	 * TRUE if this thread has been detached or joined, i.e. can be cleaned
	 * up after terminating.
	 */
	bool detached_or_joined;

	/**
	 * TRUE if the threads has terminated (canceled, via thread_exit or
	 * returned from the main function)
	 */
	bool terminated;

};

/**
 * Next thread ID.
 */
static u_int next_id;

/**
 * Mutex to safely access the next thread ID.
 */
static mutex_t *id_mutex;

/**
 * Store the thread object in a thread-specific value.
 */
static thread_value_t *current_thread;

bool thread_cancelability(bool enable)
{
	int old;

	pthread_setcancelstate(enable ? PTHREAD_CANCEL_ENABLE
								  : PTHREAD_CANCEL_DISABLE, &old);

	return old == PTHREAD_CANCEL_ENABLE;
}

int thread_current_id()
{
	private_thread_t *this = (private_thread_t*)thread_current();

	return this ? this->id : 0;
}

void thread_cleanup_push(thread_cleanup_t cleanup, void *arg)
{
	private_thread_t *this = (private_thread_t*)thread_current();
	cleanup_handler_t *handler;

    struct thread_clean_entry *entry = calloc(1, sizeof(*entry));
    entry->handler.cleanup = cleanup;
    entry->handler.arg = arg;

    TAILQ_INSERT_HEAD(&this->cleanup_handlers, entry, entries);
}

void thread_cleanup_pop(bool execute)
{
	private_thread_t *this = (private_thread_t*)thread_current();
	cleanup_handler_t *handler;

    struct thread_clean_entry *entry = TAILQ_FIRST(&this->cleanup_handlers);
    
    TAILQ_REMOVE(&this->cleanup_handlers, entry, entries);

    handler = &entry->handler;

	if (execute)
	{
		handler->cleanup(handler->arg);
	}
	free(entry);
}

static void thread_destroy(private_thread_t *this)
{
	if (!this->terminated || !this->detached_or_joined)
	{
		this->mutex->unlock(this->mutex);
		return;
	}
	this->mutex->unlock(this->mutex);
	this->mutex->destroy(this->mutex);
	free(this);
}

static void _cancel(thread_t *public)
{
    private_thread_t *this = (private_thread_t *)public;

    this->mutex->lock(this->mutex);
	if (pthread_equal(this->thread_id, pthread_self()))
	{
		this->mutex->unlock(this->mutex);
		fprintf(stderr, "!!! CANNOT CANCEL CURRENT THREAD !!!");
		return;
	}

	pthread_cancel(this->thread_id);
	this->mutex->unlock(this->mutex);
}

static void _kill(thread_t *public, int sig)
{
    private_thread_t *this = (private_thread_t *)public;

    this->mutex->lock(this->mutex);
	if (pthread_equal(this->thread_id, pthread_self()))
	{
		/* it might actually be possible to send a signal to pthread_self (there
		 * is an example in raise(3) describing that), the problem is though,
		 * that the thread only returns here after the signal handler has
		 * returned, so depending on the signal, the lock might not get
		 * unlocked. */
		this->mutex->unlock(this->mutex);
		fprintf(stderr, "!!! CANNOT SEND SIGNAL TO CURRENT THREAD !!!");
		return;
	}
	pthread_kill(this->thread_id, sig);
	this->mutex->unlock(this->mutex);
}

static void _detach(thread_t *public)
{
    private_thread_t *this = (private_thread_t *)public;

    this->mutex->lock(this->mutex);
	pthread_detach(this->thread_id);
	this->detached_or_joined = true;
	thread_destroy(this);
}

static void *_join(thread_t *public)
{
	pthread_t thread_id;
	void *val;
    private_thread_t *this = (private_thread_t *)public;

	this->mutex->lock(this->mutex);
	if (pthread_equal(this->thread_id, pthread_self()))
	{
		this->mutex->unlock(this->mutex);
		fprintf(stderr, "!!! CANNOT JOIN CURRENT THREAD !!!");
		return NULL;
	}
	if (this->detached_or_joined)
	{
		this->mutex->unlock(this->mutex);
		fprintf(stderr, "!!! CANNOT JOIN DETACHED THREAD !!!");
		return NULL;
	}
	thread_id = this->thread_id;
	this->detached_or_joined = true;
	if (this->terminated)
	{
		/* thread has terminated before the call to join */
		thread_destroy(this);
	}
	else
	{
		/* thread_destroy is called when the thread terminates normally */
		this->mutex->unlock(this->mutex);
	}
	pthread_join(thread_id, &val);

	return val;
}

static private_thread_t *thread_create_internal()
{
	private_thread_t *this;

    this = calloc(1, sizeof(*this));
    this->public.cancel = _cancel;
    this->public.kill = _kill;
    this->public.detach = _detach;
    this->public.join = _join;

    this->mutex = mutex_create(MUTEX_TYPE_DEFAULT);
    TAILQ_INIT(&this->cleanup_handlers);

	return this;
}

static u_int get_thread_id()
{
	int id;

	id_mutex->lock(id_mutex);
	id = next_id++;
	id_mutex->unlock(id_mutex);

	return id;
}

thread_t *thread_current()
{
	private_thread_t *this;

	this = (private_thread_t*)current_thread->get(current_thread);
	if (!this)
	{
		this = thread_create_internal();
		this->id = get_thread_id();
		current_thread->set(current_thread, (void*)this);
	}
	return &this->public;
}

static void thread_cleanup_popall_internal(private_thread_t *this)
{
    struct thread_clean_entry *curr = NULL;
    struct thread_clean_entry *next;

    curr = TAILQ_FIRST(&this->cleanup_handlers);
    while (curr)
    {
        next = TAILQ_NEXT(curr, entries);
        TAILQ_REMOVE(&this->cleanup_handlers, curr, entries);
        curr->handler.cleanup(curr->handler.arg);
        free(curr);
		curr = next;
    }
}

static void thread_cleanup(private_thread_t *this)
{
	thread_cleanup_popall_internal(this);
	this->mutex->lock(this->mutex);
	this->terminated = true;
	thread_destroy(this);
}

static void *thread_main(private_thread_t *this)
{
	void *res;

	this->id = get_thread_id();

	current_thread->set(current_thread, this);
	pthread_cleanup_push((thread_cleanup_t)thread_cleanup, this);

	printf( "created thread %.2d\n", this->id);

	res = this->main(this->arg);
	pthread_cleanup_pop(true);

	return res;
}

thread_t *thread_create(thread_main_t main, void *arg)
{
    private_thread_t *this = thread_create_internal();

    this->main = main;
	this->arg = arg;

    if (pthread_create(&this->thread_id, NULL, (void*)thread_main, this) != 0)
	{
		fprintf(stderr, "failed to create thread!");
		this->mutex->lock(this->mutex);
		this->terminated = true;
		this->detached_or_joined = true;
		thread_destroy(this);
		return NULL;
	}

	return &this->public;
}

/**
 * A dummy thread value that reserved pthread_key_t value "0". A buggy PKCS#11
 * library mangles this key, without owning it, so we allocate it for them.
 */
static thread_value_t *dummy1;

void threads_init()
{
	private_thread_t *main_thread = thread_create_internal();

	dummy1 = thread_value_create(NULL);

	next_id = 0;
	main_thread->thread_id = pthread_self();
	current_thread = thread_value_create(NULL);
	current_thread->set(current_thread, (void*)main_thread);
	id_mutex = mutex_create(MUTEX_TYPE_DEFAULT);
	main_thread->id = get_thread_id();
}

void threads_deinit()
{
	private_thread_t *main_thread = (private_thread_t*)thread_current();

	dummy1->destroy(dummy1);

	main_thread->mutex->lock(main_thread->mutex);
	main_thread->terminated = true;
	main_thread->detached_or_joined = true;
	thread_destroy(main_thread);
	current_thread->destroy(current_thread);
	id_mutex->destroy(id_mutex);
}