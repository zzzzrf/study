# 简单的线程池设计文档



## 一. 目的

​	创建一个线程池管理模块，他仅需要满足如下功能即可：

* 创建一个支持N线程的线程池
* 销毁一个线程池
* 线程池中的线程数量可调整
  * 向线程池中增加线程
  * 从线程池中减少线程
* 可以向线程池中添加任务



## 二. 总体设计

​	每个线程交给一个worker的结构体进行管理，创建一个线程池就是创建一个worker队列，在对线程池进行CRUD时需要使用线程锁来保护数据。

​	我们希望创建的线程阻塞在一个名为“job add”的信号上，只有我们为线程池添加任务的时候，线程才会去真正执行这个任务。

​	我们将新创建的任务称为job，并存储在一个名为job的队列中，当为线程池添加任务的时候，就把任务入队，并且广播这个事件。

​	线程池中线程数量的调整是一个特殊的job。

​	

## 三. 详细设计

### 3.1 结构体设计

​	我们将从小到大，来设计整个模块中的结构体。

#### 1. job

​	job用来描述用户提供的任务，每个任务都有自己的执行状态、执行函数、销毁函数。所以我们将他设计为如下结构

~~~c
typedef struct job_t job_t;

struct job_t
{
    int status;
    bool (*cancel)(job_t *this);
    void (*execute)(job_t *this);
    void (*destory)(job_t *this);
};
~~~

​	为了管理多任务，我们job保存在一个队列中

~~~c
typedef struct job_queue job_q_t;

struct job_entry
{
    job_t job;
    TAILQ_ENTRY(job_entry);
};

TAILQ_HEAD(job_queue, job_entry);
~~~



#### 2. thread

​	我们希望有一个结构体可以用来描述一个线程

~~~c
typedef void *(*thread_main_t)(void *arg);
typedef struct thread_t thread_t;

struct thread_t
{
    pthread_t tid;
    thread_main_t main;
    void *arg;
};
~~~



#### 3. worker

​	worker将会把一个线程和一个job进行关联，同时，为了每个线程方便地了解整个线程池的状态（例如锁状态、信号量状态、期望的线程数量等），也会保留一个指向线程池的指针。

~~~c
struct worker_thread_t
{
    private_processor_t *processor;
    job_t *job;
    thread_t tid;
};
~~~

​	我们需要一个worker队列来管理每一个worker，这里使用的<sys/queue.h>提供的TAILQ队列。

~~~c
typedef struct thread_queue thread_q_t;

struct worker_entry
{
    worker_thread_t worker;
    TAILQ_ENTRY(worker_entry) entries;
};

TAILQ_HEAD(thread_queue, worker_entry);
~~~



#### 4. 线程池

​	线程池需要一些统计量来保存当前线程池状态，例如当前线程池线程数量，期望线程数量，一个管理线程的队列，一个管理任务的队列，一个保护线程池的锁，一个新增任务的信号。

~~~c
typedef struct processor_t processor_t;

struct processor_t
{
    /* public interface */
    
    
	/* private member */
    int total_threads;
    int desired_threads;
    thread_q_t threads;
    job_q_t jobs;
    pthread_mutex_t mutex;
    pthread_cond_t job_add;
};
~~~



### 3.2 接口设计

​		本节将从上到下来设计所需要的接口

#### 1. 线程池

​	创建线程池，取消所有worker线程，销毁线程池，配置线程池：

~~~shell
processor_t *processor_create();

struct processor_t
{
    /* public interface */
    void (*set_threads)(processor_t *this, int count);
    void (*cancel)(processor_t *this);
    void (*destory)(processor_t *this);
    
	/* private member */
};
~~~

​	set_threads()将创建count个worker线程，每个worker线程阻塞等待job add信号。这里需要注意，不论是创建worker还是期望释放线程，都会广播job_add信号，结合process_jobs函数（后面会提到），就可以实现线程数的调整。

~~~c
static void __processor_set_threads(processor_t *this, int count)
~~~



![image-20231110170146608](README.assets/image-20231110170146608.png)

#### 2. worker

​	创建worker

~~~c
static struct worker_entry *worker_create(processor_t *processor, thread_main_t main)
~~~

​	在这个函数中，我们会对worker_entry结构体中的字段进行初始化，核心的动作是创建一个线程实例，该线程实例将会调用第二个参数main所指的回调函数。

​	由于我们的worker要做的事情是等待job_add的信号，有job的时候去处理他，这也是线程池中的一个核心函数，我们会从job队列中取出job，并且去执行这个job。

​	由于在set_threads的时候会广播job_add信号，所以所有空闲的线程都会收到，如果发现我们期望释放这个线程，那么这个线程就会退出，实现了线程数量的调整。

~~~c
static void *process_jobs(worker_thread_t *worker)
~~~

![image-20231110170118453](README.assets/image-20231110170118453.png)



#### 3. thread

​	这里仅仅是对pthread_create()的一层封装，期间会为thread_t结构体做初始化。有了这一层，我们可以对thread进行更好的控制，例如对线程清理函数的注册等，当然这些都是后话，未来会逐渐完善这些东西，目前我们只是打印当前线程的TID而已。

~~~c
static thread_t *thread_create(thread_main_t main, worker_thread_t *worker);
~~~

