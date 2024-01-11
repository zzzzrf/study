#ifndef __MY_DAVICI_T__
#define __MY_DAVICI_T__

enum packet_type {
	CMD_REQUEST = 0,
	CMD_RESPONSE = 1,
	CMD_UNKNOWN = 2,
	EVENT_REGISTER = 3,
	EVENT_UNREGISTER = 4,
	EVENT_CONFIRM = 5,
	EVENT_UNKNOWN = 6,
	EVENT = 7,
};

struct tester;
struct conn;
struct response;
struct request;

enum fdops {
	/** request read-ready notifications */
	READ = (1<<0),
	/** request write-ready notifications */
	WRITE = (1<<1),
};

typedef void (*callback)(struct conn *conn, int err, const char *name, struct response *res, void *user);
typedef void (*tester_srvcb)(struct tester *tester, int fd);
typedef int (*fdcb)(struct conn *conn, int fd, int ops, void *user);


struct tester* tester_create(tester_srvcb srvcb);
int tester_iocb(struct conn *c, int fd, int ops, void *user);
void tester_runio(struct tester *t, struct conn *c);
void tester_complete(struct tester *t);
const char *tester_getpath(struct tester *tester);
void tester_cleanup(struct tester *t);

int connect_unix(const char *path, fdcb fdcb, void *user, struct conn **cp);


int new_cmd(const char *cmd, struct request **rp);
int queue(struct conn *c, struct request *r, callback cmd_cb, void *user);
#endif