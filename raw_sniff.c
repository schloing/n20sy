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

#define NETWARN(message) fprintf(stderr, message);

char* ipv4_protocol(unsigned int code);

void  process_ipv4(unsigned char data[], uint8_t* source, uint8_t* dest);
void  process_ipv6(unsigned char data[], uint8_t* source, uint8_t* dest);

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

        // https://www.ionos.com/digitalguide/server/know-how/ethernet-frame/
        // https://www.flukenetworks.com/blog/cabling-chronicles/101-series-ethernet-back-basics

        uint8_t*      source = malloc(6);
        uint8_t*      dest   = malloc(6);
        uint8_t*      eth_t  = malloc(2);
        unsigned char data[1500];

        memcpy(source, buffer,      6);
        memcpy(dest,   buffer + 6,  6);
        memcpy(eth_t,  buffer + 12, 2);
        memcpy(data,   buffer + 14, 1500);

        uint16_t eth_type; // eth_t (uint8_t*) -> eth_type (uint16_t)

        // https://en.wikipedia.org/wiki/EtherType#Examples
        // https://www.iana.org/assignments/ieee-802-numbers/ieee-802-numbers.xhtml#ieee-802-numbers-1
        
        eth_type |= (eth_t[0] << 8);
        eth_type |= (eth_t[1]);

        switch (eth_type) {
        case 0x0800: process_ipv4(data, source, dest); break;
        case 0x86dd: process_ipv6(data, source, dest); break;
        default:     NETWARN("potential unimplemented protocol (saw unfamiliar EtherType)\n");
        }

        free(source);
        free(dest);
    }

    return 0;
}

#define PRINT_PROTOCOL(major, protofunc, OFF)                   \
    printf("%-5s %5s packet: ", major, protofunc(data[OFF]));   \
    printf("%.2x:%.2x:%.2x:%.2x:%.2x:%.2x > ", source[0], source[1], source[2], source[3], source[4], source[5]); \
    printf("%.2x:%.2x:%.2x:%.2x:%.2x:%.2x  ",  dest[0],   dest[1],   dest[2],   dest[3],   dest[4],   dest[5]);
     

void process_ipv4(unsigned char data[], uint8_t* source, uint8_t* dest) {
    PRINT_PROTOCOL("ipv4", ipv4_protocol, 9);
    printf("%d.%d.%d.%d:%d > ", data[12], data[13], data[14], data[15], (data[20] << 8) + data[21]);
    printf("%d.%d.%d.%d:%d\n",  data[16], data[17], data[18], data[19], (data[22] << 8) + data[23]);
}

void process_ipv6(unsigned char data[], uint8_t* source, uint8_t* dest) {
    // need to test this on a machine with ipv6
    // 64 -> +256 are source and dest addresses
    PRINT_PROTOCOL("ipv6", ipv4_protocol, 6);
    printf("[%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x]:%02x > ",
            data[8],  data[9],  data[10], data[11], data[12], data[13], data[14], data[15], data[16], 0);
    printf("[%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x]:%02x\n",
            data[24], data[25], data[26], data[27], data[28], data[29], data[30], data[31], data[32], 0);
}

char* ipv4_protocol(unsigned int code) {
    switch(code) {
        case  0x01: return "icmp";
        case  0x02: return "igmp";
        case  0x06: return "tcp";
        case  0x11: return "udp";
        case  0x29: return "ipv6"; // what?
        default:    NETWARN("potential unimplemented ipv4 protocol (saw unfamiliar protocol number in ipv4 header)\n");
                    return "unknown";
    }
}
