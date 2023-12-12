#include <errno.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define neterr(expression, msg) \
        do { if (expression == -1) perror(msg); } while (0) 

void nl_state(struct sockaddr_nl* sa, struct nlmsghdr* h) {
    struct ifinfomsg* ifi;
    char              ifname[256];

    ifi = NLMSG_DATA(h);
    if_indextoname(ifi->ifi_index, ifname);

    // https://man7.org/linux/man-pages/man7/rtnetlink.7.html
    
    printf("interface %s %s\n", ifname, (ifi->ifi_flags & IFF_RUNNING) ? "UP" : "DOWN");
}

void handle(struct sockaddr_nl* sa, struct nlmsghdr* h) {
    switch (h->nlmsg_type) {
    case RTM_NEWADDR:
        printf("RTM_NEWADDR\n");
        break;
    case RTM_DELADDR:
        printf("RTM_DELADDR\n");
        break;
    case RTM_NEWROUTE:
        printf("RTM_NEWROUTE\n");
        break;
    case RTM_DELROUTE:
        printf("RTM_DELROUTE\n");
        break;
    case RTM_NEWLINK:
        printf("RTM_NEWLINK\n");
        nl_state(sa, h);
        break;
    case RTM_DELLINK:
        printf("RTM_DELLINK\n");
        break;
    default:
        printf("unknown nlmsg_type %d\n",
               h->nlmsg_type);
        break;
    }
}

int event(struct sockaddr_nl* sa, int fd) {
    size_t             len;
    char               buf[8192];
    struct iovec       iov = { buf, sizeof(buf) };
    struct msghdr      msg = { sa, sizeof(*sa), &iov, 1, NULL, 0, 0 };
    struct nlmsghdr*   nh;

    len = recvmsg(fd, &msg, 0);
    neterr(len, "recvmsg");

    if (len == 0) {
        // man recvmsg:
        // "If no messages are available to be received and the peer has performed an orderly shutdown, recvmsg() shall return 0."
        printf("orderly shutdown\n");
        exit(EXIT_SUCCESS);
    }

    for (nh = (struct nlmsghdr*) buf; NLMSG_OK(nh, len);
         nh = NLMSG_NEXT(nh, len))
    {
        if (nh->nlmsg_type == NLMSG_DONE)
            return 0;
  
        if (nh->nlmsg_type == NLMSG_ERROR)
            return EXIT_FAILURE;

        handle(sa, nh);
    }

    return 0;
}

int main() {
    struct sockaddr_nl sa;
    int                fd;

    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;
    sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;

    neterr((fd = socket(AF_NETLINK,
            SOCK_RAW, NETLINK_ROUTE)), "socket");

    neterr(bind(fd, (struct sockaddr*)&sa,
            sizeof(sa)), "bind");

    while(1) event(&sa, fd);

    close(fd);

    return 0;
}
