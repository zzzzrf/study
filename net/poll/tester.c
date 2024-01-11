#include "tester.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stddef.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <fcntl.h>

enum tester_fd {
	FD_CLIENT = 0,
	FD_LISTEN = 1,
	FD_SERVER = 2,
	FD_COUNT
};

struct request
{
    char *buf;
    size_t used;
    callback cb;
    void *user;
};

struct packet
{
    unsigned int received;
    char buf[1024];
};

struct event
{
    struct event *next;
    callback *cb;
    void *user;
    char name[0];
};

struct conn
{
    int s;
    struct request *reqs;
    struct event *events;
    struct packet pkt;
    fdcb fdcb;
    void *user;
    enum fdops ops;
};

struct response {
	struct packet pkt;
};

struct tester {
	struct pollfd pfd[FD_COUNT];
	const char *path;
	tester_srvcb srvcb;
	int complete;
};

int tester_iocb(struct conn *c, int fd, int ops, void *user)
{
	struct tester *t = user;

	t->pfd[FD_CLIENT].fd = fd;
	t->pfd[FD_CLIENT].events = 0;
	if (ops & READ)
	{
		t->pfd[FD_CLIENT].events |= POLLIN;
	}
	if (ops & WRITE)
	{
		t->pfd[FD_CLIENT].events |= POLLOUT;
	}
	return 0;
}

struct tester *tester_create(tester_srvcb srvcb)
{
    struct sockaddr_un addr;
    struct tester *t;
    int len;

    t = calloc(1, sizeof(*t));
    t->path = "/tmp/test.sock";
    t->srvcb = srvcb;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", t->path);
    len = offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path);

    t->pfd[FD_LISTEN].fd = socket(AF_UNIX, SOCK_STREAM, 0);
    t->pfd[FD_LISTEN].events = POLLIN;
    t->pfd[FD_SERVER].fd = -1;
    t->pfd[FD_SERVER].events = POLLIN;

    unlink(t->path);
    if (bind(t->pfd[FD_LISTEN].fd, (struct sockaddr *)&addr, len) < 0)
    {
        fprintf(stderr, "bind failed\n");
        close(t->pfd[FD_LISTEN].fd );
        free(t);
        return NULL;
    }
    listen(t->pfd[FD_LISTEN].fd, 2);

    return t;
}

static int update_ops(struct conn *c, enum fdops ops)
{
	int ret;

	if (ops == c->ops)
	{
		return 0;
	}
	ret = c->fdcb(c, c->s, ops, c->user);
	if (ret == 0)
	{
		c->ops = ops;
	}
	return -abs(ret);
}

void tester_complete(struct tester *t)
{
	t->complete = 1;
}

void tester_runio(struct tester *t, struct conn *c)
{
    int err = 0;
    while (!t->complete)
    {
        int fd;
        poll(t->pfd, sizeof(t->pfd)/sizeof(t->pfd[0]), -1);
        if (t->pfd[FD_CLIENT].revents & POLLIN)
        {
            struct response res;
            printf("FD_CLIENT read : ");
            res.pkt.received = recv(t->pfd[FD_CLIENT].fd, res.pkt.buf, sizeof(res.pkt.buf), 0);
            printf("'%s'\n", res.pkt.buf);
            c->reqs->cb(c, err, "do client callback function", &res, t);
        }
        if (t->pfd[FD_CLIENT].revents & POLLOUT)
        {
            printf("FD_CLIENT write : ");
            write(c->s, c->reqs->buf, c->reqs->used);
            printf("'%s'\n", c->reqs->buf);
            update_ops(c, c->ops | READ);
            update_ops(c, c->ops & ~WRITE);
        }
        if (t->pfd[FD_LISTEN].revents & POLLIN)
        {
            printf("FD_LISTEN new client ");
            fd = accept(t->pfd[FD_LISTEN].fd, NULL, NULL);
            t->pfd[FD_SERVER].fd = fd;
            printf("%d\n", fd);
        }
        if (t->pfd[FD_SERVER].revents & POLLIN)
        {
            t->srvcb(t, t->pfd[FD_SERVER].fd);
        }
    }
}

const char *tester_getpath(struct tester *t)
{
	return t->path;
}

void tester_cleanup(struct tester *t)
{
	close(t->pfd[FD_LISTEN].fd);
	close(t->pfd[FD_SERVER].fd);
	free(t);
}

static int connect_and_fcntl(int fd, const char *path)
{
	struct sockaddr_un addr;
	int len, flags;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path);

	if (connect(fd, (struct sockaddr*)&addr, len) != 0)
	{
		return -errno;
	}
	flags = fcntl(fd, F_GETFL);
	if (flags == -1)
	{
		return -errno;
	}
#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		return -errno;
	}
	return 0;
}

int connect_unix(const char *path, fdcb fdcb, void *user, struct conn **cp)
{
    struct conn *c;

    c = calloc(1, sizeof(*c));
    c->fdcb = fdcb;
    c->user = user;

    c->s = socket(AF_UNIX, SOCK_STREAM, 0);
    connect_and_fcntl(c->s, path);
    *cp = c;

    return 0;
}

static int create_request(enum packet_type type, const char *name, struct request **rp)
{
    struct request *req;
    
    req = calloc(1, sizeof(*req));
    req->used = strlen(name) + 1;
    req->buf = malloc(req->used);
    memcpy(req->buf, name, req->used);
    *rp = req;
    return 0;
}

int new_cmd(const char *cmd, struct request **rp)
{
	return create_request(CMD_REQUEST, cmd, rp);
}

int queue(struct conn *c, struct request *r, callback cmd_cb, void *user)
{
    r->cb = cmd_cb;
    r->user = user;

    c->reqs = r;

    return update_ops(c, c->ops | WRITE);
}
