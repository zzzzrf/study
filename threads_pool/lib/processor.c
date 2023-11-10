#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/queue.h>

#include "processor.h"
#include "thread.h"
#include "mutex.h"
#include "condvar.h"
#include "job.h"

typedef struct private_processor_t private_processor_t;

static int reserved[JOB_PRIO_MAX] = 
{
    [JOB_PRIO_CRITICAL] = 0,
    [JOB_PRIO_HIGH]     = 0,
    [JOB_PRIO_MEDIUM]   = 0,
    [JOB_PRIO_LOW]      = 0,
};

typedef struct {
	private_processor_t *processor;
	thread_t *thread;
	job_t *job;
	job_priority_t priority;
} worker_thread_t;

struct worker_entry
{
    worker_thread_t worker;
    TAILQ_ENTRY(worker_entry) entries;
};

TAILQ_HEAD(threadlist, worker_entry);

struct job_entry
{
    job_t job;
    TAILQ_ENTRY(job_entry) entries;
};

TAILQ_HEAD(joblist, job_entry);

struct private_processor_t {
	processor_t public;
	int total_threads;
	int desired_threads;
	int working_threads[JOB_PRIO_MAX];
    struct threadlist threads;
    struct joblist jobs[JOB_PRIO_MAX];
	int prio_threads[JOB_PRIO_MAX];
	mutex_t *mutex;
	condvar_t *job_added;
	condvar_t *thread_terminated;
};

static int get_idle_threads_nolock(private_processor_t *this)
{
    int count;

    count = this->total_threads;
    for (int i = 0; i < JOB_PRIO_MAX; i++)
        count -= this->working_threads[i];

    return count;
}

static int _get_total_threads(processor_t *public)
{
    private_processor_t *this = (private_processor_t *)public;
    int count;

    this->mutex->lock(this->mutex);
    count = this->total_threads;
    this->mutex->unlock(this->mutex);

    return count;
}

int _get_idle_threads(processor_t *public)
{
    private_processor_t *this = (private_processor_t *)public;
    int count;

    this->mutex->lock(this->mutex);
    count = get_idle_threads_nolock(this);
    this->mutex->unlock(this->mutex);

    return count;
}

static void *_cb_process_jobs(struct worker_entry *entry);

static void restart(worker_thread_t *worker)
{
	private_processor_t *this = worker->processor;
	job_t *job;

	fprintf(stderr, "terminated worker thread %.2u", thread_current_id());

	this->mutex->lock(this->mutex);
	/* cleanup worker thread  */
	this->working_threads[worker->priority]--;
	worker->job->status = JOB_STATUS_CANCELED;
	job = worker->job;
	/* unset the job before releasing the mutex, otherwise cancel() might
	 * interfere */
	worker->job = NULL;
	/* release mutex to avoid deadlocks if the same lock is required
	 * during queue_job() and in the destructor called here */
	this->mutex->unlock(this->mutex);
	job->destroy(job);
	this->mutex->lock(this->mutex);

	/* respawn thread if required */
	if (this->desired_threads >= this->total_threads)
	{
        struct worker_entry *entry = calloc(1, sizeof(*entry));
        entry->worker.processor = this;

		entry->worker.thread = thread_create((thread_main_t)_cb_process_jobs, entry);
		if (entry->worker.thread)
		{
            TAILQ_INSERT_TAIL(&this->threads, entry, entries);
            this->mutex->unlock(this->mutex);
			return;
		}
		free(entry);
	}
	this->total_threads--;
	this->thread_terminated->signal(this->thread_terminated);
	this->mutex->unlock(this->mutex);
}

static void process_job(private_processor_t *this, worker_thread_t *worker)
{
	job_t *to_destroy = NULL;
	job_requeue_t requeue;

	this->working_threads[worker->priority]++;
	worker->job->status = JOB_STATUS_EXECUTING;
	this->mutex->unlock(this->mutex);
	/* canceled threads are restarted to get a constant pool */
	thread_cleanup_push((thread_cleanup_t)restart, worker);
	while (true)
	{
		requeue = worker->job->execute(worker->job);
		if (requeue.type != JOB_REQUEUE_TYPE_DIRECT)
		{
			break;
		}
		else if (!worker->job->cancel)
		{	/* only allow cancelable jobs to requeue directly */
			requeue.type = JOB_REQUEUE_TYPE_FAIR;
			break;
		}
	}
	thread_cleanup_pop(false);
	this->mutex->lock(this->mutex);
	this->working_threads[worker->priority]--;
	if (worker->job->status == JOB_STATUS_CANCELED)
	{	/* job was canceled via a custom cancel() method or did not
		 * use JOB_REQUEUE_TYPE_DIRECT */
		to_destroy = worker->job;
	}
	else
	{
		switch (requeue.type)
		{
			case JOB_REQUEUE_TYPE_NONE:
				worker->job->status = JOB_STATUS_DONE;
				to_destroy = worker->job;
				break;
			case JOB_REQUEUE_TYPE_FAIR:
            #if 0
				worker->job->status = JOB_STATUS_QUEUED;
				this->jobs[worker->priority]->insert_last(
									this->jobs[worker->priority], worker->job);
				this->job_added->signal(this->job_added);
            #endif
				break;
			case JOB_REQUEUE_TYPE_SCHEDULE:
				/* scheduler_t does not hold its lock when queuing jobs
				 * so this should be safe without unlocking our mutex */
                #if 0
				switch (requeue.schedule)
				{
					case JOB_SCHEDULE:
						lib->scheduler->schedule_job(lib->scheduler,
												worker->job, requeue.time.rel);
						break;
					case JOB_SCHEDULE_MS:
						lib->scheduler->schedule_job_ms(lib->scheduler,
												worker->job, requeue.time.rel);
						break;
					case JOB_SCHEDULE_TV:
						lib->scheduler->schedule_job_tv(lib->scheduler,
												worker->job, requeue.time.abs);
						break;
				}
                #endif
				break;
			default:
				break;
		}
	}
	/* unset the current job to avoid interference with cancel() when
	 * destroying the job below */
	worker->job = NULL;

	if (to_destroy)
	{	/* release mutex to avoid deadlocks if the same lock is required
		 * during queue_job() and in the destructor called here */
		this->mutex->unlock(this->mutex);
		to_destroy->destroy(to_destroy);
		this->mutex->lock(this->mutex);
	}
}

static bool get_job(private_processor_t *this, worker_thread_t *worker)
{
	int i, reserved = 0, idle;

	idle = get_idle_threads_nolock(this);

	for (i = 0; i < JOB_PRIO_MAX; i++)
	{
		if (reserved && reserved >= idle)
		{
			fprintf(stderr, "delaying %d priority jobs: %d threads idle, "
				 "but %d reserved for higher priorities", i, idle, reserved);
			/* wait until a job of higher priority gets queued */
			return false;
		}
		if (this->working_threads[i] < this->prio_threads[i])
		{
			reserved += this->prio_threads[i] - this->working_threads[i];
		}

        struct job_entry *first = TAILQ_FIRST(&this->jobs[i]);
		if (first)
        {
			TAILQ_REMOVE(&this->jobs[i], first, entries);
        	worker->job = &first->job; 
        	worker->priority = i;
			return true;
		}
	}
	return false;
}

static void *_cb_process_jobs(struct worker_entry *entry)
{
    private_processor_t *this = entry->worker.processor;

	/* worker threads are not cancelable by default */
	thread_cancelability(false);

	printf( "started worker thread %.2u\n", thread_current_id());

	this->mutex->lock(this->mutex);
	while (this->desired_threads >= this->total_threads)
	{
		if (get_job(this, &entry->worker))
		{
			process_job(this, &entry->worker);
		}
		else
		{
			this->job_added->wait(this->job_added, this->mutex);
		}
	}
	this->total_threads--;
	this->thread_terminated->signal(this->thread_terminated);
	this->mutex->unlock(this->mutex);
	return NULL;
}

void _set_threads(processor_t *public, int count)
{
    private_processor_t *this = (private_processor_t *)public;

    this->mutex->lock(this->mutex);
    for (int i = 0; i < JOB_PRIO_MAX; i++)
    {
        this->prio_threads[i] = reserved[i];
    }

    if (count > this->total_threads)
    {
        /* increase */
        struct worker_entry *entry;

        this->desired_threads = count;
        for (int i = this->total_threads; i < count; i++)
        {
            entry = calloc(1, sizeof(*entry));
            entry->worker.processor = this;
            entry->worker.thread = thread_create((thread_main_t)_cb_process_jobs, entry);

            if (entry->worker.thread)
			{
                TAILQ_INSERT_TAIL(&this->threads, entry, entries);
				this->total_threads++;
			}
            else 
            {
                free(entry);
            }
        }
    }
    else if (count < this->total_threads)
    {
        /* decrease */
        this->desired_threads = count;
    }
    this->job_added->broadcast(this->job_added);
    this->mutex->unlock(this->mutex);
}

processor_t *processor_create()
{
    private_processor_t *this;

    this = malloc(sizeof(*this));
    memset(this, 0, sizeof(*this));

	this->public.get_total_threads = _get_total_threads;
    this->public.get_idle_threads = _get_idle_threads;
    this->public.set_threads = _set_threads;

    this->mutex = mutex_create(MUTEX_TYPE_DEFAULT);
    this->job_added = condvar_create(CONDVAR_TYPE_DEFAULT);
    this->thread_terminated = condvar_create(CONDVAR_TYPE_DEFAULT);
    
    TAILQ_INIT(&this->threads);

    for (int i = 0; i < JOB_PRIO_MAX; i++)
        TAILQ_INIT(&this->jobs[i]);

    return &this->public;
}