#ifndef PACKET_CREATION_H
#define PACKET_CREATION_H

#include <stddef.h>

// Global variables for tracking our source IP/port for response matching
extern char g_source_ip[16];
extern int g_source_port;

// Function to get the appropriate source IP for a destination
int get_source_ip(const char *dest_ip, char *source_ip);

// Function to create a raw TCP packet
// Parameters:
//   target: Target IP address as string
//   port: Target port number
//   timeout: Timeout value (currently unused, for future enhancement)
//   flags: TCP flags (bit 0=SYN, bit 1=RST, bit 2=PSH, bit 3=ACK, bit 4=URG)
//   packet_data: Output parameter - pointer to allocated packet data
//   packet_size: Output parameter - size of the allocated packet
// Returns: 0 on success, -1 on error
int create_packet(const char *target, int port, int timeout, int flags, 
                  char **packet_data, size_t *packet_size);

// Checksum calculation function for IP and TCP headers
unsigned short checksum(void *b, int len);

#endif // PACKET_CREATION_H