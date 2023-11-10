#ifndef __THREADS_POOL_00_H__
#define __THREADS_POOL_00_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/queue.h>

typedef void *(*thread_main_t)(void *arg);

typedef struct processor_t processor_t;
typedef struct thread_t thread_t;
typedef struct worker_thread_t worker_thread_t;
typedef struct job_t job_t;
typedef struct job_queue job_q_t;
typedef struct thread_queue thread_q_t;

struct thread_t
{
    pthread_t tid;
    thread_main_t main;
    void *arg;
};

struct job_t
{
    int status;
    bool (*cancel)(job_t *this);
    void (*execute)(job_t *this);
    void (*destory)(job_t *this);
};

struct job_entry
{
    job_t job;
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

TAILQ_HEAD(thread_queue, worker_entry);

struct processor_t
{
    /* public interface */
    void (*set_threads)(processor_t *this, int count);
    void (*cancel)(processor_t *this);
    void (*destory)(processor_t *this);
    
	/* private member */
    int total_threads;
    int desired_threads;
    thread_q_t threads;
    job_q_t jobs;
    pthread_mutex_t mutex;
    pthread_cond_t job_add;
    pthread_cond_t thread_terminated;
};

processor_t *processor_create();

#endif