#include "threads_pool_00.h"
#include <pthread.h>
#include <stdio.h>
#include <sys/queue.h>
#include <unistd.h>

static void *thread_main(thread_t *this)
{
    void *res;

    printf( "created thread %ld\n", this->tid);
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
        job_del->job.destory(&job_del->job);
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
    return false;
}

static void process_job(processor_t *processor, worker_thread_t *worker)
{
    return ;
}

static void *process_jobs(worker_thread_t *worker)
{
    processor_t *processor = worker->processor;
    printf( "started worker thread %ld\n", worker->thread->tid);
    pthread_mutex_lock(&processor->mutex);

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
    printf( "end worker thread %ld\n", worker->thread->tid);
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

processor_t *processor_create()
{
    processor_t *this = NULL;

    this = calloc(1, sizeof(*this));
    this->set_threads = __processor_set_threads;
    this->cancel = __processor_cancel;
    this->destory = __processor_destory;

    pthread_mutex_init(&this->mutex, NULL);
    pthread_cond_init(&this->job_add, NULL);
    pthread_cond_init(&this->thread_terminated, NULL);

    TAILQ_INIT(&this->threads);
    TAILQ_INIT(&this->jobs);

    return this;
}

int main(void)
{
    processor_t *processor = processor_create();

    processor->set_threads(processor, 4);

    sleep(1);

    processor->destory(processor);

    return 0;
}