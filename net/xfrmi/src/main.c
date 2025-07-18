#include <stdio.h>
#include <stdlib.h>

#include "nl_xfrmi.h"

#define XFRMI_NAME  "xfrmi0"
#define XFRMI_PHY   "enxc8a362028361"
#define XFRMI_MTU   1200

int main(int argc, char *argv[])
{
    nl_xfrmi_t *xfrmi = nl_xfrmi_create();
    if (xfrmi == NULL) {
        printf("xfrmi create failed\n");
        return -1;
    }

    if (xfrmi->create(xfrmi, XFRMI_NAME, 443, XFRMI_PHY, XFRMI_MTU) != 0) {
        printf("xfrmi create failed\n");
        goto destory;
    }

    if (xfrmi->up(xfrmi, XFRMI_NAME) != 0) {
        printf("xfrmi up failed\n");
        goto destory;
    }

    if (xfrmi->delete(xfrmi, XFRMI_NAME) != 0) {
        printf("xfrmi delete failed\n");
        goto destory;
    }

destory:
    xfrmi->destroy(xfrmi);
    return 0;
}