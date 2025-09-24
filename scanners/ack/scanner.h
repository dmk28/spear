#ifndef ACK_SCAN_H
#define ACK_SCAN_H

// Global quiet mode flag
extern int g_quiet_mode;

int ack_scan(const char *target_ip, int port);
int ack_scan_range(const char *target_ip, int start_port, int end_port);
#endif
