#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <net/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if_link.h>

#include "uthash.h"
#include "nl_xfrmi.h"

typedef struct nl_xfrmi_mgr_t
{
	char name[IFNAMSIZ];
	nl_xfrmi_t *xfrmi;
	UT_hash_handle hh;
}nl_xfrmi_mgr_t;

typedef struct private_nl_xfrmi_t private_nl_xfrmi_t;
typedef struct nl_socket_t nl_socket_t;

struct nl_socket_t
{
    int fd;
    int (*send_ack)(nl_socket_t *this, struct nlmsghdr *in);
    void (*destroy)(nl_socket_t *this);
};

struct private_nl_xfrmi_t {
	nl_xfrmi_t public;
    nl_socket_t *socket;
};

typedef union {
	struct nlmsghdr hdr;
	u_char bytes[1024];
} netlink_buf_t __attribute__((aligned(RTA_ALIGNTO)));

nl_xfrmi_mgr_t *xfrmi_tables = NULL;

static void nl_add_attribute(struct nlmsghdr *hdr, int rta_type, unsigned char *ptr, size_t len,
						  size_t buflen)
{
	struct rtattr *rta;

	if (NLMSG_ALIGN(hdr->nlmsg_len) + RTA_LENGTH(len) > buflen)
	{
		fprintf(stderr, "unable to add attribute, buffer too small\n");
		return;
	}

	rta = (struct rtattr*)(((char*)hdr) + NLMSG_ALIGN(hdr->nlmsg_len));
	rta->rta_type = rta_type;
	rta->rta_len = RTA_LENGTH(len);
	memcpy(RTA_DATA(rta), ptr, len);
	hdr->nlmsg_len = NLMSG_ALIGN(hdr->nlmsg_len) + RTA_ALIGN(rta->rta_len);
}

static struct rtattr *nl_nested_start(struct nlmsghdr *hdr, int buflen, int type)
{
	struct rtattr *rta;

	if (NLMSG_ALIGN(hdr->nlmsg_len) + RTA_LENGTH(0) > buflen)
	{
		fprintf(stderr, "unable to add attribute, buffer too small\n");
		return NULL;
	}

	rta = ((void*)hdr) + NLMSG_ALIGN(hdr->nlmsg_len);
	rta->rta_type = type;
	rta->rta_len = RTA_LENGTH(0);
	hdr->nlmsg_len = NLMSG_ALIGN(hdr->nlmsg_len) + RTA_ALIGN(rta->rta_len);
	return rta;
}

static void nl_nested_end(struct nlmsghdr *hdr, void *attr)
{
	struct rtattr *rta = attr;
	void *end;

	if (attr)
	{
		end = (char*)hdr + NLMSG_ALIGN(hdr->nlmsg_len);
		rta->rta_len = end - attr;
	}
}

static int _nl_socket_send_ack(nl_socket_t *this, struct nlmsghdr *msg)
{
	int len;
	fd_set set;
	char buf[1024] = {};
	struct timeval tv = {};
	struct nlmsghdr *hdr;
	struct sockaddr_nl addr = {
		.nl_family = AF_NETLINK,
	};
	
    len = sendto(this->fd, msg, msg->nlmsg_len, 0,
					 (struct sockaddr*)&addr, sizeof(addr));
    if (len != msg->nlmsg_len)
    {
        fprintf(stderr, "sendto failed: %d, errno: %s(%d)\n", len, strerror(errno), errno);
		return -1;
    }

	FD_ZERO(&set);
	FD_SET(this->fd, &set);
	tv.tv_sec = 1;

	if (select(this->fd + 1, &set, NULL, NULL, &tv) <= 0)
	{
		fprintf(stderr, "select failed: %s(%d)\n", strerror(errno), errno);
		return -1;
	}

	len = recv(this->fd, buf, sizeof(buf), MSG_TRUNC);
	if (len < 0)
	{
		fprintf(stderr, "recv failed: %s(%d)\n", strerror(errno), errno);
		return -1;
	}

	hdr = (struct nlmsghdr*)buf;
	while (NLMSG_OK(hdr, len))
	{
		switch (hdr->nlmsg_type)
		{
			case NLMSG_ERROR:
			{
				struct nlmsgerr *err = NLMSG_DATA(hdr);

				if (err->error)
				{
					if (-err->error == EEXIST)
						return 3;
					if (-err->error == ESRCH)
						return 6;
					return -1;
				}
				return 0;
			}
			default:
				hdr = NLMSG_NEXT(hdr, len);
				continue;
			case NLMSG_DONE:
				break;
		}
		break;
	}
    return 0;
}

static int _nl_xfrmi_create(nl_xfrmi_t *public, char *name,
                    unsigned int if_id, char *phys, unsigned int mtu)
{
    netlink_buf_t request;
    private_nl_xfrmi_t *this = (private_nl_xfrmi_t*)public;
	struct nlmsghdr *hdr;
	struct ifinfomsg *msg;
	struct rtattr *linkinfo, *info_data;
	unsigned int ifindex = 0;

	if (phys)
	{
		ifindex = if_nametoindex(phys);
		if (!ifindex)
		{
			fprintf(stderr, "physical interface '%s' not found\n", phys);
			return -1;
		}
	}

	memset(&request, 0, sizeof(request));
	hdr = &request.hdr;
	hdr->nlmsg_pid = getpid();
	hdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_EXCL;
	hdr->nlmsg_type = RTM_NEWLINK;
	hdr->nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));

	msg = NLMSG_DATA(hdr);
	msg->ifi_family = AF_UNSPEC;

    nl_add_attribute(hdr, IFLA_IFNAME, 
                (unsigned char *)name, strlen(name), sizeof(request));

	if (mtu)
        nl_add_attribute(hdr, IFLA_MTU, 
                (unsigned char *)&mtu, sizeof(mtu), sizeof(request));

    linkinfo = nl_nested_start(hdr, sizeof(request), IFLA_LINKINFO);

	nl_add_attribute(hdr, IFLA_INFO_KIND, (unsigned char *)"xfrm", strlen("xfrm"),
						  sizeof(request));

    info_data = nl_nested_start(hdr, sizeof(request), IFLA_INFO_DATA);

    nl_add_attribute(hdr, IFLA_XFRM_IF_ID, (unsigned char *)&if_id, sizeof(if_id),
						  sizeof(request));

	if (ifindex)
		nl_add_attribute(hdr, IFLA_XFRM_LINK, (unsigned char *)&ifindex, sizeof(ifindex),
							  sizeof(request));

	nl_nested_end(hdr, info_data);
	nl_nested_end(hdr, linkinfo);

	switch (this->socket->send_ack(this->socket, hdr))
	{
		case 0:
			return this->public.up(&this->public, name);
		case 3:
			fprintf(stderr, "XFRM interface '%s' already exists\n", name);
			break;
		default:
			fprintf(stderr, "failed to create XFRM interface '%s'\n", name);
			break;
	}
    return -1;
}

static int _nl_xfrmi_up(nl_xfrmi_t *public, char *name)
{
	netlink_buf_t request;
	struct nlmsghdr *hdr;
	struct ifinfomsg *msg;
	private_nl_xfrmi_t *this = (private_nl_xfrmi_t*)public;

	memset(&request, 0, sizeof(request));

	hdr = &request.hdr;
	hdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	hdr->nlmsg_type = RTM_SETLINK;
	hdr->nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));

	msg = NLMSG_DATA(hdr);
	msg->ifi_family = AF_UNSPEC;
	msg->ifi_change |= IFF_UP;
	msg->ifi_flags |= IFF_UP;

	nl_add_attribute(hdr, IFLA_IFNAME, (unsigned char *)name, strlen(name),
						  sizeof(request));

	if (this->socket->send_ack(this->socket, hdr) != 0)
	{
		fprintf(stderr, "failed to bring up XFRM interface '%s'", name);
		return -1;
	}
	return 0;
}

static int _nl_xfrmi_delete(nl_xfrmi_t *public, char *name)
{
	struct nlmsghdr *hdr;
	struct ifinfomsg *msg;
	struct rtattr *linkinfo;
	private_nl_xfrmi_t *this = (private_nl_xfrmi_t*)public;
	netlink_buf_t request;

	memset(&request, 0, sizeof(request));

	hdr = &request.hdr;
	hdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	hdr->nlmsg_type = RTM_DELLINK;
	hdr->nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));

	msg = NLMSG_DATA(hdr);
	msg->ifi_family = AF_UNSPEC;

    nl_add_attribute(hdr, IFLA_IFNAME, 
			(unsigned char *)name, strlen(name), sizeof(request));
	
	linkinfo = nl_nested_start(hdr, sizeof(request), IFLA_LINKINFO);
	nl_add_attribute(hdr, IFLA_INFO_KIND, (unsigned char *)"xfrm", strlen("xfrm"), sizeof(request));
	nl_nested_end(hdr, linkinfo);

	switch (this->socket->send_ack(this->socket, hdr))
	{
		case 0:
            return 0;
		case 6:
			fprintf(stderr, "XFRM interface '%s' not found to delete\n", name);
		default:
			fprintf(stderr, "failed to delete XFRM interface '%s'\n", name);
			break;
	}
    return -1;
}

static void _nl_xfrmi_destory(nl_xfrmi_t *public)
{
	private_nl_xfrmi_t *this = (private_nl_xfrmi_t*)public;
	if (this->socket)
	{
		this->socket->destroy(this->socket);
	}
	free(this);
}

static void _nl_socket_destory(nl_socket_t *this)
{
	if (this->fd != -1)
	{
		close(this->fd);
	}
	free(this);
}

static nl_socket_t *nl_socket_create(int protocol)
{
    nl_socket_t *this;
    this = calloc(1, sizeof(*this));
    this->fd = socket(AF_NETLINK, SOCK_RAW, protocol);

    if (this->fd < 0)
    {
        free(this);
        return NULL;
    }

    this->send_ack = _nl_socket_send_ack;
    this->destroy = _nl_socket_destory;
    return this;
}

nl_xfrmi_t *nl_xfrmi_create()
{
	private_nl_xfrmi_t *this;

    this = calloc(1, sizeof(*this));
    this->public.create = _nl_xfrmi_create;
	this->public.up = _nl_xfrmi_up;
    this->public.delete = _nl_xfrmi_delete;
	this->public.destroy = _nl_xfrmi_destory;
    this->socket = nl_socket_create(NETLINK_ROUTE);

	if (!this->socket)
	{
		free(this);
		return NULL;
	}
	return &this->public;
}