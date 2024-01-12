#include "tester.h"
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>

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

static void client_cb(struct conn *c, int err, const char *name,
				  struct response *res, void *user)
{
    printf("\n[%s][%d] %s\n", __func__, __LINE__, name);
    tester_complete(user);
}

int main(int argc, char **argv)
{
	struct tester *t;
    struct conn *c;
    struct request *r;

    t = tester_create(server_cb);
    connect_unix(tester_getpath(t), tester_iocb, t, &c);
    new_cmd("hello echocb server", &r);
    queue(c, r, client_cb, t);
    tester_runio(t, c);
    return 0;
}
