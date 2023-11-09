#ifndef __MY_PROCESSOR_H__
#define __MY_PROCESSOR_H__

typedef struct processor_t processor_t;

struct processor_t {

	int (*get_total_threads) (processor_t *this);

	int (*get_idle_threads) (processor_t *this);

	/**
	 * Get the number of threads currently working, per priority class.
	 *
	 * @param				priority to check
	 * @return				number of threads in priority working
	 */
	// int (*get_working_threads)(processor_t *this, job_priority_t prio);

	/**
	 * Get the number of queued jobs for a specified priority.
	 *
	 * @param prio			priority class to get job load for
	 * @return				number of items in queue
	 */
	// int (*get_job_load) (processor_t *this, job_priority_t prio);

	/**
	 * Adds a job to the queue.
	 *
	 * This function is non blocking and adds a job_t to the queue.
	 *
	 * @param job			job to add to the queue
	 */
	// void (*queue_job) (processor_t *this, job_t *job);

	/**
	 * Directly execute a job with an idle worker thread.
	 *
	 * If no idle thread is available, the job gets executed by the calling
	 * thread.
	 *
	 * @param job			job, gets destroyed
	 */
	// void (*execute_job)(processor_t *this, job_t *job);

	/**
	 * Set the number of threads to use in the processor.
	 *
	 * If the number of threads is smaller than number of currently running
	 * threads, thread count is decreased. Use 0 to disable the processor.
	 *
	 * This call does not block and wait for threads to terminate if the number
	 * of threads is reduced.  Instead use cancel() for that during shutdown.
	 *
	 * @param count			number of threads to allocate
	 */
	void (*set_threads)(processor_t *this, int count);

	/**
	 * Sets the number of threads to 0 and cancels all blocking jobs, then waits
	 * for all threads to be terminated.
	 */
	void (*cancel)(processor_t *this);

	/**
	 * Destroy a processor object.
	 */
	void (*destroy) (processor_t *processor);
};

processor_t *processor_create();

#endif