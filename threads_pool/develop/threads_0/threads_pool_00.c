#include "threads_pool_00.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <unistd.h>

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
    srand(time(NULL) + pthread_self());
    int timeout = rand()%10;
    sleep(timeout < 1 ? 1 : timeout);
    printf("%ld [%s]\n", pthread_self(), __func__);
    requeue.type = JOB_REQUEUE_TYPE_FAIR;
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
    processor_t *processor = processor_create();

    processor->set_threads(processor, 4);

    struct job_entry *entry1 = calloc(1, sizeof(*entry1));
    job_t *job1 = calloc(1, sizeof(*job1));
    job1->execute = __job_exec;
    job1->destory = __job_destory;
    job1->cancel  = __job_cancel;
    entry1->job = job1;

    struct job_entry *entry2 = calloc(1, sizeof(*entry2));
    job_t *job2 = calloc(1, sizeof(*job2));
    job2->execute = __job_exec;
    job2->destory = __job_destory;
    job2->cancel  = __job_cancel;
    entry2->job = job2;

    struct job_entry *entry3 = calloc(1, sizeof(*entry3));
    job_t *job3 = calloc(1, sizeof(*job3));
    job3->execute = __job_exec;
    job3->destory = __job_destory;
    job3->cancel  = __job_cancel;
    entry3->job = job3;

    struct job_entry *entry4 = calloc(1, sizeof(*entry4));
    job_t *job4 = calloc(1, sizeof(*job4));
    job4->execute = __job_exec;
    job4->destory = __job_destory;
    job4->cancel  = __job_cancel;
    entry4->job = job4;

    processor->queue_job(processor, entry1);
    processor->queue_job(processor, entry2);
    processor->queue_job(processor, entry3);
    processor->execute_job(processor, entry4);

    while(!getchar());
    processor->destory(processor);

    return 0;
}