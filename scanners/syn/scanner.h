#ifndef SCANNER_H
#define SCANNER_H

// Function to perform a SYN scan on a single port
// Returns: 1 if port is open, 0 if closed, -1 on error or filtered
int syn_scan(const char *target_ip, int port);

// Function to scan a range of ports
// Returns: number of open ports found
int syn_scan_range(const char *target_ip, int start_port, int end_port);

#endif // SCANNER_H