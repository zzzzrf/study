---
title: 异步IO框架
date: 2024-1-13 9:16:43
tags:
    - 进程间通信
    - poll
categories: 网络编程
description: 
cover: blog_image/XFRM-state/ipsec.png 
---

## 一. 简介

## 二. 异步IO

本文不再介绍同步/异步IO的使用场景，如需回顾，请移步阅读[异步IO](https://zrfyun.top/2024/01/07/asynchronousIO/)。

本节主要介绍`poll()`函数。下文内容多数翻译自`man poll`

### 2.1 函数原型

```c
#include <poll.h>
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
```

`poll()`就像`select()`一样，它等待一组文件描述符准备执行I/O。文件描述符被存储在类型为`struct pollfd`的数组中。

```c
struct pollfd {
    int   fd;         /* file descriptor */
    short events;     /* requested events */
    short revents;    /* returned events */
};
```

events字段是入参，表示fd所关注的事件，revents字段是输出，由内核填充实际发生了的事件。
如果没有发生任何请求的事件（并且没有错误），`poll()`会阻塞，直到其中一个事件发生。

可以在event和revent中设置/返回的位是在`<poll.h>`中定义的：

```c
#define POLLIN  0x001  /* There is data to read.  */
#define POLLPRI  0x002  /* There is urgent data to read.  */
#define POLLOUT  0x004  /* Writing now will not block.  */
```

## 三. 框架

### 3.1 文件描述符

这个程序的特色就是他即使客户端，又是服务端。为了实现这两个角色，需要用到三个fd。分别是服务端的listen fd(FD_LISTEN)、accept fd(FD_SERVER)，客户端的client fd(FD_CLIENT)。

```c
enum tester_fd {
 FD_CLIENT = 0,
 FD_LISTEN = 1,
 FD_SERVER = 2,
 FD_COUNT
};
```

所以我们`poll()`函数的第一个入参将是这三个fd的`struct pollfd`数组。

### 3.2 主程序逻辑

我希望创建一个名为tester的后端，他将作为主程序的server，恢复来自主程序的请求。

主程序的逻辑如下：

1. 创建tester后端
2. 主程序向后端建立连接，并给tester后端注册后端收到数据后的回调函数
3. 创建一个请求
4. 注册主程序收到来自tester后端后的回调函数
5. 运行IO函数

代码如下：

```c
int main(int argc, char **argv)
{
    /* 创建后端 */
    t = tester_create(server_cb);

    /* 建立连接， 并给tester后端注册后端收到数据后的回调函数tester_iocb */
    connect_unix(tester_getpath(t), tester_iocb, t, &c);

    /* 创建一个请求 */
    new_cmd("hello echocb server", &r);

    /* 注册主程序收到来自tester后端后的回调函数client_cb */
    queue(c, r, client_cb, t);

    /* 运行IO函数 */
    tester_runio(t, c);

    return 0;
}
```

### 3.3 IO

这些fd的IO过程将是这个样子：

```c
void tester_runio(struct pollfd *pfds)
{
    while (true)
    {
        int fd;
        poll(pfds, 3, -1);
        if (pfds[FD_CLIENT].revents & POLLIN)
        {
            /* 客户端进行读操作，并执行注册的回调函数 */
        }
        if (pfds[FD_CLIENT].revents & POLLOUT)
        {
            /* 客户端进行写操作，更新fd 关注的event字段 */
        }
        if (pfds[FD_LISTEN].revents & POLLIN)
        {
            /* 服务端监听到来自客户端的连接，将fd保存在FD_SERVER中 */
            fd = accept(t->pfd[FD_LISTEN].fd, NULL, NULL);
            t->pfd[FD_SERVER].fd = fd;
        }
        if (pfds[FD_SERVER].revents & POLLIN)
        {
           /* 服务端进行写操作，执行注册的回调函数 */
        }
    }
}
```

### 3.4 回调函数

* 服务端回调函数，收到来自client的信息后，原样写回。

```c
static void server_cb(struct tester *t, int fd)
{
    char buf[1024];
    uint32_t len;

    printf("\n[%s][%d] do server callback function\n", __func__, __LINE__);
    len = read(fd, buf, sizeof(buf));
    printf("FD_SERVER read '%s'\n", buf);
    printf("FD_SERVER write '%s' back\n\n", buf);
    write(fd, buf, len);
}
```

* 客户端回调函数，收到来此服务端的相应后执行的动作。

```c
static void client_cb(struct conn *c, int err, const char *name,
                            struct response *res, void *user)
{
    printf("\n[%s][%d] %s\n", __func__, __LINE__, name);
    tester_complete(user);
}
```

### 3.5 运行效果

```bash
FD_CLIENT write : 'hello echocb server'
FD_LISTEN new client 5

[server_cb][11] do server callback function
FD_SERVER read 'hello echocb server'
FD_SERVER write 'hello echocb server' back

FD_CLIENT read : 'hello echocb server'

[client_cb][21] do client callback function
```
