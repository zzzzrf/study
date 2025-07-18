#ifndef __NL_XFRMI_H__
#define __NL_XFRMI_H__

typedef struct nl_xfrmi_t nl_xfrmi_t;
typedef struct nl_xfrmi_mgr_t nl_xfrmi_mgr_t;

struct nl_xfrmi_t
{
	int (*create)(nl_xfrmi_t *this, char *name, unsigned int if_id,
				   char *phys, unsigned int mtu);

	int (*up)(nl_xfrmi_t *public, char *name);

	int (*delete)(nl_xfrmi_t *this, char *name);

	void (*destroy)(nl_xfrmi_t *this);									
};

extern nl_xfrmi_mgr_t *xfrmi_tables;

nl_xfrmi_t *nl_xfrmi_create();

#endif