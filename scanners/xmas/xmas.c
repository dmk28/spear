#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <sys/time.h>
#include "../util/packet_creation.h"
#include <stdbool.h>
#include "xmas.h"


xmas get_next_random_xmas_flag(xmas previous_flag) {
    xmas available_flags[XMAS_FLAGS_COUNT - 1];
    int available_count = 0;

    for (int i = 0; i < XMAS_FLAGS_COUNT; i++) {
        if (XMAS_FLAGS[i] != previous_flag) {
            available_flags[available_count++] = XMAS_FLAGS[i];
        }
    }

    int random_index = rand() % available_count;
    return available_flags[random_index];
}


int ip_checksum(void *addr, int len) {
    unsigned short *ip_header = (unsigned short *)addr;
    unsigned int sum = 0;

    for (int i = 0; i < len / 2; i++) {
        sum += ip_header[i];
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}


int send_tcp_packet(char *target_ip, int port, xmas current_flag) {
    struct sockaddr_in dest_addr;
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);

    if (sock < 0) {
        perror("socket");
        return -1;
    }

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = inet_addr(target_ip);

    struct iphdr *ip_header = (struct iphdr *)calloc(1, sizeof(struct iphdr));
    struct tcphdr *tcp_header = (struct tcphdr *)calloc(1, sizeof(struct tcphdr));

    ip_header->ihl = 5;
    ip_header->version = 4;
    ip_header->tos = 0;
    ip_header->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
    ip_header->id = htons(rand());
    ip_header->frag_off = 0;
    ip_header->ttl = 64;
    ip_header->protocol = IPPROTO_TCP;
    ip_header->check = 0;
    ip_header->saddr = inet_addr("127.0.0.1");
    ip_header->daddr = dest_addr.sin_addr.s_addr;

    tcp_header->source = htons(rand());
    tcp_header->dest = dest_addr.sin_port;
    tcp_header->seq = htonl(rand());
    tcp_header->ack_seq = 0;
    tcp_header->doff = 5;
    tcp_header->fin = 0;
    tcp_header->syn = 0;
    tcp_header->rst = 0;
    tcp_header->psh = 0;
    tcp_header->ack = 0;
    tcp_header->urg = 0;

    // Set only the specific flag for this packet
    switch (current_flag) {
        case RST_FLAG:
            tcp_header->rst = 1;
            break;
        case PSH_FLAG:
            tcp_header->psh = 1;
            break;
        case URG_FLAG:
            tcp_header->urg = 1;
            break;
    }


    ip_header->check = ip_checksum((unsigned short *)ip_header, sizeof(struct iphdr));

    char packet[sizeof(struct iphdr) + sizeof(struct tcphdr)];
    memcpy(packet, ip_header, sizeof(struct iphdr));
    memcpy(packet + sizeof(struct iphdr), tcp_header, sizeof(struct tcphdr));

    free(ip_header);
    free(tcp_header);

    int sent = sendto(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    close(sock);

    return sent;
}

void perform_random_xmas_scan(const char *target_ip, int port, int num_packets) {
    xmas prev_flag = 0;
    bool first_packet = true;

    for (int i = 0; i < num_packets; i++) {
        xmas current_flag;
        if (first_packet) {
            current_flag = XMAS_FLAGS[rand() % XMAS_FLAGS_COUNT];
            first_packet = false;
        } else {
            current_flag = get_next_random_xmas_flag(prev_flag);
        }


        send_tcp_packet((char *)target_ip, port, current_flag);
        prev_flag = current_flag;
        usleep(rand() % 1000000);


    }



}
