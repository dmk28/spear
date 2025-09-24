#ifdef ACK_SCAN
#define ACK_SCAN
int ack_scan(const char *target_ip, int port);
int ack_scan_range(const char *target_ip, int start_port, int end_port);
#endif
