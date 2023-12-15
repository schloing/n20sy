#include <arpa/inet.h>
#include <linux/filter.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

char* find_protocol(unsigned int code);

#define error(emsg)                                    \
    perror(emsg);                                      \
    fprintf(stderr,                                    \
            "have you run the program"                 \
            "with administrative privileges (sudo)?\n" \
            );

#define NETERR(expr, message, clean) \
    do {                             \
        if (expr < 0) {              \
            clean;                   \
            error(message);          \
            exit(1);                 \
        }                            \
    } while(0)                       \

int main() {
    char     buffer[2048];
    int      sock;
    char*    device = "eth0";

    NETERR((sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP))), "socket", NULL);
    NETERR((setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
            device, strlen(device) + 1)), "setsockopt", NULL);

    struct ifreq ethreq;
    strncpy(ethreq.ifr_name, device, IF_NAMESIZE);
    NETERR((ioctl(sock, SIOCGIFFLAGS, &ethreq)), "ioctl", close(sock));

    ethreq.ifr_flags |= IFF_PROMISC;
    NETERR((ioctl(sock, SIOCGIFFLAGS, &ethreq)), "ioctl", close(sock));

    while (1) {
        int len = recvfrom(sock, buffer, 2048, 0, NULL, NULL);
        NETERR(len, "recvfrom()", close(sock));

        printf("%s\n", buffer);

        // https://www.ionos.com/digitalguide/server/know-how/ethernet-frame/
        // https://www.flukenetworks.com/blog/cabling-chronicles/101-series-ethernet-back-basics

        uint8_t*      source = malloc(6);
        uint8_t*      dest   = malloc(6);
        unsigned char data[1500];

        memcpy(source, buffer, 6);
        memcpy(dest,   buffer + 6, 6);
        memcpy(data,   buffer + 14, 1500);

        printf("%s packet:\n", find_protocol(data[9]));

        printf("%.2x:%.2x:%.2x:%.2x:%.2x:%.2x -> ", source[0], source[1], source[2], source[3], source[4], source[5]);
        printf("%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",   dest[0],   dest[1],   dest[2],   dest[3],   dest[4],   dest[5]);

        printf("%d.%d.%d.%d:%d -> ", data[12], data[13], data[14], data[15], (data[20]<<8) + data[21]);
        printf("%d.%d.%d.%d:%d",     data[16], data[17], data[18], data[19], (data[22]<<8) + data[23]);

        free(source);
        free(dest);
    }

    return 0;
}

char* find_protocol(unsigned int code) {
    switch(code) {
        case  1: return "icmp";
        case  2: return "igmp";
        case  6: return "tcp";
        case 17: return "udp";
        default: return "unknown";
    }
}
