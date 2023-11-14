#ifndef __THREADS_POOL_01_H__
#define __THREADS_POOL_01_H__

#include <pthread.h>
#include <stdbool.h>
#include <sys/queue.h>
#include <sys/bitypes.h>

#define HEAP_SIZE_DEFAULT 64

typedef struct processor_t processor_t;
typedef struct thread_t thread_t;
typedef struct worker_thread_t worker_thread_t;
typedef enum job_status_t job_status_t;
typedef enum job_requeue_type_t job_requeue_type_t;
typedef struct job_requeue_t job_requeue_t;
typedef struct job_t job_t;
typedef struct job_queue job_q_t;
typedef struct thread_queue thread_q_t;
typedef struct event_t event_t;
typedef struct scheduler_t scheduler_t;
typedef struct callback_job_t callback_job_t;

typedef void *(*thread_main_t)(void *arg);
typedef job_requeue_t (*callback_job_cb_t)(void *data);
typedef void (*callback_job_cleanup_t)(void *data);
typedef bool (*callback_job_cancel_t)(void *data);

struct thread_t
{
    pthread_t tid;
    thread_main_t main;
    void *arg;
};

enum job_status_t 
{
	JOB_STATUS_QUEUED = 0,
	JOB_STATUS_EXECUTING,
	JOB_STATUS_CANCELED,
	JOB_STATUS_DONE,
};

enum job_requeue_type_t {
	JOB_REQUEUE_TYPE_NONE = 0,
	JOB_REQUEUE_TYPE_FAIR,
	JOB_REQUEUE_TYPE_DIRECT,
	JOB_REQUEUE_TYPE_SCHEDULE,
};

struct job_requeue_t {
	job_requeue_type_t type;
    struct timeval time;
};

struct job_t
{
    job_status_t status;
    bool (*cancel)(job_t *this);
    job_requeue_t (*execute)(job_t *this);
    void (*destory)(job_t *this);
};

struct job_entry
{
    job_t *job;
    TAILQ_ENTRY(job_entry) entries;
};

TAILQ_HEAD(job_queue, job_entry);

struct worker_thread_t
{
    processor_t *processor;
    job_t *job;
    thread_t *thread;
};

struct worker_entry
{
    worker_thread_t worker;
    TAILQ_ENTRY(worker_entry) entries;
};

struct callback_job_t
{
    job_t job;
    callback_job_cb_t callback;
    void *data;
    callback_job_cleanup_t cleanup;
    callback_job_cancel_t cancel;
};

TAILQ_HEAD(thread_queue, worker_entry);

struct processor_t
{
    /* public interface */
    void (*set_threads)(processor_t *this, int count);
    void (*queue_job) (processor_t *this, struct job_entry *job_entry);
    void (*execute_job)(processor_t *this, struct job_entry *job_entry);
    void (*cancel)(processor_t *this);
    void (*destory)(processor_t *this);
    
	/* private member */
    int total_threads;
    int desired_threads;
    int working_threads;
    thread_q_t threads;
    job_q_t jobs;
    pthread_mutex_t mutex;
    pthread_cond_t job_add;
    pthread_cond_t thread_terminated;
};

struct event_t
{
    job_t *job;
    struct timeval time;
};

struct scheduler_t
{
    /* public interface */
    void (*schedule_job_tv)(scheduler_t *this, job_t *job, struct timeval tv);
    void (*flush)(scheduler_t *this);
    void (*destory)(scheduler_t *this);

    /* private member */
    event_t **heap;
    int heap_size;
    int event_count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

processor_t *processor_create();
scheduler_t *scheduler_create();

#endif