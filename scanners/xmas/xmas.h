#ifndef XMAS_H
#define XMAS_H

#define TIMEOUT_SEC 2

typedef enum  {
    URG_FLAG = 0x20,
    PSH_FLAG = 0x08,
    RST_FLAG = 0x04,
} xmas;


static const xmas XMAS_FLAGS[] = {
    RST_FLAG,
    PSH_FLAG,
    URG_FLAG
};

#define XMAS_FLAGS_COUNT 3



xmas get_next_random_xmas_flag(xmas previous_flag);
int ip_checksum(void *addr, int len);
int send_tcp_packet(char *target_ip, int port, xmas current_flag);
void perform_random_xmas_scan(const char *target_ip, int port, int num_packets);




#endif
