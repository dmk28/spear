#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <ifaddrs.h>

#include <netinet/tcp.h>
#include <time.h>
#include <sys/time.h>

#define PACKET_SIZE 4096

// Global variables for tracking our source IP/port for response matching
char g_source_ip[16];
int g_source_port;

// Function to get the appropriate source IP for a destination
int get_source_ip(const char *dest_ip, char *source_ip) {
    int sock;
    struct sockaddr_in dest_addr, src_addr;
    socklen_t addr_len = sizeof(src_addr);
    
    // Create a UDP socket for route discovery
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    
    // Set up destination address
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(53); // Use DNS port for route discovery
    
    if (inet_pton(AF_INET, dest_ip, &dest_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid destination IP address: %s\n", dest_ip);
        close(sock);
        return -1;
    }
    
    // Connect to determine the source IP that would be used for this destination
    if (connect(sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }
    
    // Get the local address that was assigned
    if (getsockname(sock, (struct sockaddr*)&src_addr, &addr_len) < 0) {
        perror("getsockname");
        close(sock);
        return -1;
    }
    
    // Convert to string format
    if (inet_ntop(AF_INET, &src_addr.sin_addr, source_ip, 16) == NULL) {
        perror("inet_ntop");
        close(sock);
        return -1;
    }
    
    close(sock);
    return 0;
}

// Checksum calculation function
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    // Make 16 bit words out of every two adjacent 8 bit words and 
    // calculate the sum of all 16 bit words
    for (sum = 0; len > 1; len -= 2) {
        sum += *buf++;
    }

    // Add left-over byte, if any
    if (len == 1) {
        sum += *(unsigned char*)buf << 8;
    }

    // Fold 32-bit sum to 16 bits: add carrier to result
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    result = ~sum; // One's complement
    return result;
}

int create_packet(const char *target, int port, int timeout, int flags, char **packet_data, size_t *packet_size) {
    (void)timeout; // Suppress unused parameter warning
    static int initialized = 0;
    if (!initialized) {
        srand(time(NULL));
        initialized = 1;
    }

    struct sockaddr_in addr;
    char *packet;
    struct iphdr *iph;
    struct tcphdr *tcph;
    char source_ip[16];
    
    // Get proper source IP for this destination
    if (get_source_ip(target, source_ip) != 0) {
        fprintf(stderr, "Failed to determine source IP for target %s\n", target);
        return -1;
    }
    
    // Store source IP globally for response matching
    strcpy(g_source_ip, source_ip);
    
    // Allocate memory for the packet
    packet = malloc(PACKET_SIZE);
    if (!packet) {
        fprintf(stderr, "Failed to allocate memory for packet\n");
        return -1;
    }

    memset(packet, 0, PACKET_SIZE);
    iph = (struct iphdr *)packet;
    tcph = (struct tcphdr *)(packet + sizeof(struct iphdr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, target, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid target IP address: %s\n", target);
        free(packet);
        return -1;
    }

    // Generate a random source port in the high range to avoid conflicts
    g_source_port = 32768 + (rand() % 32768);

    // Fill in the IP Header
    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
    iph->id = htons(rand() % 65535);
    iph->frag_off = 0;
    iph->ttl = 255;
    iph->protocol = IPPROTO_TCP;
    iph->check = 0; // Will be calculated later
    iph->saddr = inet_addr(source_ip); // Use proper source IP
    iph->daddr = addr.sin_addr.s_addr;

    // Fill in the TCP Header
    tcph->source = htons(g_source_port);
    tcph->dest = htons(port);
    tcph->seq = htonl(rand() % 4294967295U);
    tcph->ack_seq = 0;
    tcph->doff = 5; // TCP header size
    tcph->fin = 0;
    tcph->syn = (flags & 1) ? 1 : 0; // SYN flag
    tcph->rst = (flags & 2) ? 1 : 0; // RST flag
    tcph->psh = (flags & 4) ? 1 : 0; // PSH flag
    tcph->ack = (flags & 8) ? 1 : 0; // ACK flag
    tcph->urg = (flags & 16) ? 1 : 0; // URG flag
    tcph->window = htons(5840);
    tcph->check = 0; // Will be calculated later
    tcph->urg_ptr = 0;

    // Calculate IP header checksum
    iph->check = checksum((unsigned short *)packet, sizeof(struct iphdr));

    // For TCP checksum calculation, we need a pseudo header
    struct pseudo_header {
        u_int32_t source_address;
        u_int32_t dest_address;
        u_int8_t placeholder;
        u_int8_t protocol;
        u_int16_t tcp_length;
    };

    struct pseudo_header psh;
    psh.source_address = iph->saddr;
    psh.dest_address = iph->daddr;
    psh.placeholder = 0;
    psh.protocol = IPPROTO_TCP;
    psh.tcp_length = htons(sizeof(struct tcphdr));

    int psize = sizeof(struct pseudo_header) + sizeof(struct tcphdr);
    char *pseudogram = malloc(psize);
    if (!pseudogram) {
        fprintf(stderr, "Failed to allocate memory for pseudogram\n");
        free(packet);
        return -1;
    }

    memcpy(pseudogram, (char*)&psh, sizeof(struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header), tcph, sizeof(struct tcphdr));

    tcph->check = checksum((unsigned short*)pseudogram, psize);
    free(pseudogram);

    // Set output parameters
    *packet_data = packet;
    *packet_size = sizeof(struct iphdr) + sizeof(struct tcphdr);

    return 0; // Success
}