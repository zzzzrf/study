#include "threads_pool_01.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

static processor_t *processor;
static scheduler_t *scheduler;

time_t time_monotonic(struct timeval *tv)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
	{
		if (tv)
		{
			tv->tv_sec = ts.tv_sec;
			tv->tv_usec = ts.tv_nsec / 1000;
		}
		return ts.tv_sec;
	}
    if (!tv)
	{
		return time(NULL);
	}
    if (gettimeofday(tv, NULL) != 0)
	{	/* should actually never fail if passed pointers are valid */
		return -1;
	}
	return tv->tv_sec;
}

static job_requeue_t __callback_job_execute(job_t *public)
{
    callback_job_t *this = (callback_job_t *)public;
    return this->callback(this->data);
}

static void __callback_job_destory(job_t *public)
{
    callback_job_t *this = (callback_job_t *)public;
    if (this->cleanup)
    {
        this->cleanup(this->data);
    }
    free(this);
}

static bool __callback_job_cancel(job_t *public)
{
    callback_job_t *this = (callback_job_t *)public;
    return this->cancel(this);
}

callback_job_t *callback_job_create_with_prio(callback_job_cb_t cb, void *data,
				callback_job_cleanup_t cleanup, callback_job_cancel_t cancel)
{
    callback_job_t *this;

    this = calloc(1, sizeof(*this));
    
    this->job.execute = __callback_job_execute;
    this->job.destory = __callback_job_destory;

    this->callback = cb;
    this->data = data;
    this->cancel = cancel;
    this->cleanup = cleanup;

    if (cancel)
    {
        this->job.cancel = __callback_job_cancel;
    }

    return this;
}

static void *thread_main(thread_t *this)
{
    void *res;

    // printf( "created thread %ld\n", this->tid);
    res = this->main(this->arg);
    return res;
}

static thread_t *thread_create(thread_main_t main, worker_thread_t *worker)
{
    thread_t *this = calloc(1, sizeof(*this));

    this->main = main;
    this->arg = worker;

    if (pthread_create(&this->tid, NULL, (void *)thread_main, this) != 0)
    {
        free(this);
        return NULL;
    }
    return this;
}

static struct worker_entry *worker_create(processor_t *processor, thread_main_t main)
{
    struct worker_entry *entry = calloc(1, sizeof(*entry));

    entry->worker.processor = processor;
    entry->worker.thread = thread_create(main, &entry->worker);
    
    if (entry->worker.thread == NULL)
    {
        fprintf(stderr, "create thread failed!\n");
        free(entry);
        return NULL;
    }

    return entry;
}

static void __processor_cancel(processor_t *this)
{
    struct worker_entry *worker, *worker_del;
    struct job_entry *job, *job_del;

    pthread_mutex_lock(&this->mutex);
    this->desired_threads = 0;

    worker = TAILQ_FIRST(&this->threads);
    while (worker)
    {
        if (worker->worker.job && worker->worker.job->cancel)
        {
            if (!worker->worker.job->cancel(worker->worker.job))
            {
                pthread_cancel(worker->worker.thread->tid);
            }
        }
        worker = TAILQ_NEXT(worker, entries);
    }

    while (this->total_threads > 0)
    {
        pthread_cond_broadcast(&this->job_add);
        pthread_cond_wait(&this->thread_terminated, &this->mutex);
    }

    worker_del = TAILQ_FIRST(&this->threads);
    while (worker_del)
    {
        worker = TAILQ_NEXT(worker_del, entries);
        pthread_join(worker_del->worker.thread->tid, NULL);
        free(worker_del->worker.thread);
        free(worker_del);
        worker_del = worker;
    }

    job_del = TAILQ_FIRST(&this->jobs);
    while (job_del)
    {
        job = TAILQ_NEXT(job_del, entries);
        printf("destory\n");
        job_del->job->destory(job_del->job);
        free(job_del);
        job_del = job;
    }
    pthread_mutex_unlock(&this->mutex);
}

static void __processor_destory(processor_t *this)
{
    __processor_cancel(this);
    pthread_mutex_destroy(&this->mutex);
    pthread_cond_destroy(&this->job_add);

    free(this);
}

static bool get_job(processor_t *processor, worker_thread_t *worker)
{
    struct job_entry *job_entry;
    job_entry = TAILQ_FIRST(&processor->jobs);

    if (job_entry)
    {
        TAILQ_REMOVE(&processor->jobs, job_entry, entries);
        worker->job = job_entry->job;
        free(job_entry);
        return true;
    }
    return false;
}

static void process_job(processor_t *processor, worker_thread_t *worker)
{
    job_requeue_t requeue;
    struct job_entry *job_entry;
    job_t *this_job, *to_destory;

    this_job = worker->job;
    to_destory = NULL;
    processor->working_threads++;

    this_job->status = JOB_STATUS_EXECUTING;
    pthread_mutex_unlock(&processor->mutex);

    while (true)
    {
        requeue = this_job->execute(this_job);
        if (requeue.type != JOB_REQUEUE_TYPE_DIRECT)
        {
            break;
        }
        else if (!this_job->cancel)
        {
            requeue.type = JOB_REQUEUE_TYPE_FAIR;
            break;
        }
    }

    pthread_mutex_lock(&processor->mutex);
    processor->working_threads--;
    if (this_job->status == JOB_STATUS_CANCELED)
    {
        to_destory = this_job;
    }
    else 
    {
        switch (requeue.type) 
        {
            case JOB_REQUEUE_TYPE_NONE:
                this_job->status = JOB_STATUS_DONE;
                to_destory = this_job;
                break;
            case JOB_REQUEUE_TYPE_FAIR:
                this_job->status = JOB_STATUS_QUEUED;
                job_entry = calloc(1, sizeof(*job_entry));
                job_entry->job = this_job;
                TAILQ_INSERT_TAIL(&processor->jobs, job_entry, entries);
                pthread_cond_signal(&processor->job_add);
                break;
            case JOB_REQUEUE_TYPE_SCHEDULE:
                printf("add a timer thread\n");
                scheduler->schedule_job_tv(scheduler, this_job, requeue.time);
                break;
            default:
                break;
        }
    }

    worker->job = NULL;

    if (to_destory)
    {
        pthread_mutex_unlock(&processor->mutex);
        to_destory->destory(to_destory);
        pthread_mutex_lock(&processor->mutex);
    }

    return ;
}

static void *process_jobs(worker_thread_t *worker)
{
    processor_t *processor = worker->processor;
    
    pthread_mutex_lock(&processor->mutex);
    // printf( "started worker thread %ld\n", worker->thread->tid);

    while (processor->desired_threads >= processor->total_threads)
    {
        if (get_job(processor, worker))
        {
            process_job(processor, worker);
        }
        else 
        {
            pthread_cond_wait(&processor->job_add, &processor->mutex);
        }
    }
    processor->total_threads--;
    pthread_cond_signal(&processor->thread_terminated);
    pthread_mutex_unlock(&processor->mutex);
    // printf( "end worker thread %ld\n", worker->thread->tid);
    return NULL;
}

static void __processor_set_threads(processor_t *this, int count)
{
    pthread_mutex_lock(&this->mutex);

    if (count > this->total_threads)
    {
        struct worker_entry *entry;
        this->desired_threads = count;

        #if 0
        while (count > this->total_threads)
        {
            entry = worker_create(this, NULL);
            if (entry)
            {
                TAILQ_INSERT_TAIL(&this->threads, entry, entries);
                this->total_threads++;
            }
        }
        #endif
        for (int i = this->total_threads; i < count; i++)
        {
            entry = worker_create(this, (thread_main_t)process_jobs);
            if (entry)
            {
                TAILQ_INSERT_TAIL(&this->threads, entry, entries);
                this->total_threads++;
            }
        }
    }
    else if (count < this->total_threads) 
    {
        this->desired_threads = count;
    }

    pthread_cond_broadcast(&this->job_add);
    pthread_mutex_unlock(&this->mutex);
}

static void __processor_queue_job(processor_t *this, struct job_entry *entry)
{
    pthread_mutex_lock(&this->mutex);
    TAILQ_INSERT_TAIL(&this->jobs, entry, entries);
    pthread_cond_signal(&this->job_add);
    pthread_mutex_unlock(&this->mutex);
}

static void  __processor_execute_job(processor_t *this, struct job_entry *entry)
{
    bool queued = false;

    pthread_mutex_lock(&this->mutex);
    if (this->desired_threads && (this->total_threads > this->working_threads))
    {   
        entry->job->status = JOB_STATUS_QUEUED;
        TAILQ_INSERT_HEAD(&this->jobs, entry, entries);
        queued = true;
    }
    pthread_cond_signal(&this->job_add);
    pthread_mutex_unlock(&this->mutex);
    
    if (!queued)
    {
        entry->job->execute(entry->job);
        entry->job->destory(entry->job);
        free(entry);
    }
}

static event_t *remove_event(scheduler_t *this)
{
    event_t *top, *event;

    if (!this->event_count)
        return NULL;

    event = this->heap[1];
    top = this->heap[1] = this->heap[this->event_count];

    if (--this->event_count > 1)
    {
        int position = 1;

        while ((position << 1) <= this->event_count)
        {
            int child = position << 1;

            if ((child + 1) <= this->event_count && 
                    timercmp(&this->heap[child + 1]->time, &this->heap[child]->time, <))
            {
                child++;
            }

            if (!timercmp(&top->time, &this->heap[child]->time, >))
            {
                break;
            }

            this->heap[position] = this->heap[child];
            position =child;
        }

        this->heap[position] = top;
    }
    return event;
}

static void __scheduler_job_tv(scheduler_t *this, job_t *job, struct timeval tv)
{
    event_t *event;
    int position;

    event = calloc(1, sizeof(*event));
    event->job = job;
    event->time = tv;

    job->status = JOB_STATUS_QUEUED;

    pthread_mutex_lock(&this->mutex);
    this->event_count++;

    if (this->event_count > this->heap_size)
    {
        this->heap_size <<= 1;
        this->heap = realloc(this->heap, sizeof(event_t *) * (this->heap_size + 1));
    }

    position = this->event_count;
    while (position > 1 && timercmp(&this->heap[position >> 1]->time, &event->time, >))
    {
        this->heap[position] = this->heap[position >> 1];
		position >>= 1;
    }
    this->heap[position] = event;
    pthread_cond_signal(&this->cond);
    pthread_mutex_unlock(&this->mutex);
}

static void __scheduler_flush(scheduler_t *this)
{
    event_t *event;
    pthread_mutex_lock(&this->mutex);
    while ((event = remove_event(this)) != NULL)
    {
        event->job->destory(event->job);
        free(event);
    }
    pthread_mutex_unlock(&this->mutex);
}

static void __scheduler_destory(scheduler_t *this)
{
    __scheduler_flush(this);
    pthread_mutex_destroy(&this->mutex);
    pthread_cond_destroy(&this->cond);
    free(this->heap);
    free(this);
}

static event_t *peek_event(scheduler_t * this)
{
    return (this->event_count > 0) ? this->heap[1] : NULL;
}

static job_requeue_t __scheduler_schedule(scheduler_t * this)
{
    job_requeue_t requeue = {.type = JOB_REQUEUE_TYPE_DIRECT};
    int ret;
    struct job_entry *entry;
    struct timeval now;
    event_t *event;
    bool timed = false;

    pthread_mutex_lock(&this->mutex);
    // ret = pthread_mutex_trylock(&this->mutex);
    // perror(strerror(ret));

    time_monotonic(&now);

    if ((event = peek_event(this)) != NULL)
    {
        if (!timercmp(&now, &event->time, <))
        {
            remove_event(this);
            pthread_mutex_unlock(&this->mutex);
            printf("got event, queuing job for execution\n");
            entry = calloc(1, sizeof(*entry));
            entry->job = event->job;
            processor->queue_job(processor, entry);
            free(event);
            return requeue;
        }
        timersub(&event->time, &now, &now);
        if (now.tv_sec)
            printf("next event in %lds %ldms, waiting\n", now.tv_sec, now.tv_usec/1000);
        else
            printf("next event in %ldms, waiting\n", now.tv_usec/1000);

        timed = true;
    }

    if (timed)
    {
        struct timespec ts;
        ts.tv_sec = event->time.tv_sec;
        ts.tv_nsec = event->time.tv_usec * 1000;
        pthread_cond_timedwait(&this->cond, &this->mutex, &ts);
    }
    else 
    {
        printf("no events, waiting\n");
        pthread_cond_wait(&this->cond, &this->mutex);
    }

    pthread_mutex_unlock(&this->mutex);
    return requeue;
}

static bool ___scheduler_cb_cancel()
{
    return false;
}

scheduler_t *scheduler_create()
{
    scheduler_t *this = calloc(1, sizeof(*this));
    callback_job_t *job = NULL;
    struct job_entry *entry;

    this->heap_size = HEAP_SIZE_DEFAULT;
    this->heap = calloc(this->heap_size + 1, sizeof(event_t *));

    this->schedule_job_tv = __scheduler_job_tv;
    this->flush = __scheduler_flush;
    this->destory = __scheduler_destory;

    pthread_mutex_init(&this->mutex, NULL);
    // pthread_cond_init(&this->cond, NULL);
    pthread_condattr_t condattr;
    pthread_condattr_init(&condattr);
    pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);
    pthread_cond_init(&this->cond, &condattr);
    pthread_condattr_destroy(&condattr);

    job = callback_job_create_with_prio((callback_job_cb_t)__scheduler_schedule, this, 
                                        NULL, ___scheduler_cb_cancel);
    entry = calloc(1, sizeof(*entry));
    entry->job = (job_t *)job;

    processor->queue_job(processor, entry);

    return this;
}

processor_t *processor_create()
{
    processor_t *this = NULL;
    this = calloc(1, sizeof(*this));

    this->set_threads = __processor_set_threads;
    this->queue_job = __processor_queue_job;
    this->execute_job = __processor_execute_job;
    this->cancel = __processor_cancel;
    this->destory = __processor_destory;

    pthread_mutex_init(&this->mutex, NULL);
    pthread_cond_init(&this->job_add, NULL);
    pthread_cond_init(&this->thread_terminated, NULL);

    TAILQ_INIT(&this->threads);
    TAILQ_INIT(&this->jobs);

    return this;
}

static job_requeue_t __job_exec(job_t *this)
{
    job_requeue_t requeue;
    struct timeval tv;
    time_monotonic(&tv);
    tv.tv_sec += 3;

    printf("%ld [%s]\n", pthread_self(), __func__);
    requeue.type = JOB_REQUEUE_TYPE_SCHEDULE;
    requeue.time = tv;
    return requeue;
}

static void __job_destory(job_t *this)
{
    printf("[%s]\n", __func__);
    free(this);
}

static bool __job_cancel(job_t *this)
{
    printf("[%s]\n", __func__);
    return true;
}

int main(void)
{
    processor = processor_create();

    processor->set_threads(processor, 2);
    
    scheduler = scheduler_create();

    job_t *job;

    job = calloc(1, sizeof(*job));
    job->execute = __job_exec;
    job->destory = __job_destory;
    job->cancel = __job_cancel;

    struct timeval tv;

    time_monotonic(&tv);

    tv.tv_sec += 2;
    scheduler->schedule_job_tv(scheduler, job, tv);

    while(!getchar());
    // pause();
    scheduler->destory(scheduler);
    processor->destory(processor);

    return 0;
}