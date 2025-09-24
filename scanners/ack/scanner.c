
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
#include "scanner.h"

#define TIMEOUT_SEC 2








int ack_scan(const char *target_ip, int port) {
    int raw_sock;
    struct sockaddr_in target_addr;
    int flag = 2;
    char *packet_data = NULL;
    size_t packet_size;
    int result;


    raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);

    if (raw_sock < 0) {
        perror("Socket failed to initialize");
        return -1;
    }

    if (setsockopt(raw_sock, IPPROTO_IP, IP_HDRINCL, &flag, sizeof(flag)) < 0) {
        perror("setsockopt failed");
        close(raw_sock);
        return -1;
    }

    struct timeval timeout;

    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;

    if (setsockopt(raw_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt  SO_RCVTIMEO failed");
        close(raw_sock);
        return -1;
    }
    result = create_packet(target_ip, port, TIMEOUT_SEC, 6, &packet_data, &packet_size);
    if (result < 0 || packet_data == NULL) {
        fprintf(stderr, "Failed to create packet\n");
        close(raw_sock);
        return -1;
    }

    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, target_ip, &target_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address: %s\n", target_ip);
        free(packet_data);
        close(raw_sock);
        return -1;
    }

    if (sendto(raw_sock, packet_data, packet_size, 0, (struct sockaddr *)&target_addr, sizeof(target_addr)) < 0) {
        perror("sendto failed");
        free(packet_data);
        close(raw_sock);
        return -1;
    }

    // Packet sent silently

    char recv_buffer[4096];
    struct sockaddr_in recv_addr;
    socklen_t recv_addr_len = sizeof(recv_addr);
    ssize_t recv_len = recvfrom(raw_sock, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr *)&recv_addr, &recv_addr_len);


    if (recv_len > 0) {
        struct iphdr *recv_ip = (struct iphdr *)recv_buffer;
        struct tcphdr *recv_tcp = (struct tcphdr *)(recv_buffer + recv_ip->ihl * 4);


        if (recv_addr.sin_addr.s_addr == target_addr.sin_addr.s_addr && ntohs(recv_tcp->source) == port) {

            if (recv_tcp->ack == 1) {
                if (!g_quiet_mode) {
                    printf("Port %d is open\n", port);
                }
                result =1;
            } else if (recv_tcp->rst == 1) {
                // Port is closed - don't print anything
                result =0;
            } else {
                if (!g_quiet_mode) {
                    printf("Port %d gave unknown answer\n", port);
                }
                result =-1;
            }
        } else {
            // No response - likely filtered, don't print anything
            result = -1;
        }
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Timeout - don't print anything for filtered ports
        } else {
            perror("recvfrom failed");

        }
        result = -1;
    }

    free(packet_data);
    close(raw_sock);

    return result;


    //
}


int ack_scan_range(const char *target_ip, int start_port, int end_port) {
    int open_count = 0;

    // Scanning quietly - only show open ports

    for (int port = start_port; port <= end_port; port++) {
        int result = ack_scan(target_ip, port);
        if (result == 1) {
            open_count++;
        }

        // Small delay between scans to avoid overwhelming the target
        usleep(10000); // 10ms delay
    }

    if (open_count > 0) {
        printf("\nScan complete. Found %d open ports.\n", open_count);
    } else {
        printf("No open ports found.\n");
    }
    return open_count;
}
