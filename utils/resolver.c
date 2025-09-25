#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>

// Check if a string is a valid IP address
extern int is_valid_ip(const char *ip_str) {
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;
    
    // Try IPv4 first
    if (inet_pton(AF_INET, ip_str, &(sa.sin_addr)) == 1) {
        return 1;
    }
    
    // Try IPv6
    if (inet_pton(AF_INET6, ip_str, &(sa6.sin6_addr)) == 1) {
        return 1;
    }
    
    return 0; // Not a valid IP
}

extern char* retrieve_ip_address(struct sockaddr *addr, socklen_t addrlen) {
    (void)addrlen; // Suppress unused parameter warning
    static char ipstr[INET6_ADDRSTRLEN]; // Use larger buffer for IPv6
    void *addr_ptr;

    if (addr->sa_family == AF_INET) {
        addr_ptr = &((struct sockaddr_in *)addr)->sin_addr;
        inet_ntop(AF_INET, addr_ptr, ipstr, INET6_ADDRSTRLEN);
    } else if (addr->sa_family == AF_INET6) {
        addr_ptr = &((struct sockaddr_in6 *)addr)->sin6_addr;
        inet_ntop(AF_INET6, addr_ptr, ipstr, INET6_ADDRSTRLEN);
    } else {
        fprintf(stderr, "Unsupported address family: %d\n", addr->sa_family);
        return NULL;
    }

    return ipstr;
}

extern char* resolve_hostname(const char *hostname) {
    struct addrinfo hints, *res;
    int status;
    char *ip_address = NULL;

    // First check if it's already an IP address
    if (is_valid_ip(hostname)) {
        static char ip_copy[INET6_ADDRSTRLEN];
        strncpy(ip_copy, hostname, sizeof(ip_copy) - 1);
        ip_copy[sizeof(ip_copy) - 1] = '\0';
        return ip_copy;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // Prefer IPv4 for now
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return NULL;
    }

    if (res != NULL) {
        ip_address = retrieve_ip_address(res->ai_addr, res->ai_addrlen);
        printf("Resolved %s to %s\n", hostname, ip_address ? ip_address : "unknown");
    }

    freeaddrinfo(res);
    return ip_address;
}
