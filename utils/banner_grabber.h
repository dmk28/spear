#ifndef BANNER_GRABBER_H
#define BANNER_GRABBER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>

#define MAX_BANNER_SIZE 4096
#define MAX_HOST_SIZE 256
#define MAX_SERVICE_SIZE 64
#define BANNER_TIMEOUT 10
#define MAX_RESULTS 10000

// Banner result structure
typedef struct {
    char host[MAX_HOST_SIZE];
    int port;
    char service[MAX_SERVICE_SIZE];
    char banner[MAX_BANNER_SIZE];
    time_t timestamp;
    int success;
} banner_result_t;

// Banner grabber configuration
typedef struct {
    int timeout;
    int verbose;
    char *output_file;
    char *db_file;
    int max_results;
} banner_config_t;

// Collection of banner results
typedef struct {
    banner_result_t *results;
    int count;
    int capacity;
} banner_collection_t;

// Function declarations
banner_config_t* create_banner_config(void);
void free_banner_config(banner_config_t *config);

banner_collection_t* create_banner_collection(int initial_capacity);
void free_banner_collection(banner_collection_t *collection);
int add_banner_result(banner_collection_t *collection, const banner_result_t *result);

int grab_banner(const char *host, int port, banner_result_t *result, int timeout);
int grab_banner_with_probe(const char *host, int port, const char *probe, banner_result_t *result, int timeout);

// Service-specific banner grabbers
int grab_http_banner(const char *host, int port, banner_result_t *result, int timeout);
int grab_ftp_banner(const char *host, int port, banner_result_t *result, int timeout);
int grab_ssh_banner(const char *host, int port, banner_result_t *result, int timeout);
int grab_smtp_banner(const char *host, int port, banner_result_t *result, int timeout);
int grab_telnet_banner(const char *host, int port, banner_result_t *result, int timeout);
int grab_pop3_banner(const char *host, int port, banner_result_t *result, int timeout);
int grab_imap_banner(const char *host, int port, banner_result_t *result, int timeout);

// Service detection
const char* detect_service(int port);
int is_http_port(int port);
int is_ssl_port(int port);

// Utility functions
int connect_with_timeout(const char *host, int port, int timeout);
int send_probe_and_read(int sockfd, const char *probe, char *buffer, int buffer_size, int timeout);
void clean_banner_text(char *banner);
void format_banner_for_display(const banner_result_t *result, char *output, int output_size);

// Export and database functions
int export_results_to_text(const banner_collection_t *collection, const char *filename);
int export_results_to_csv(const banner_collection_t *collection, const char *filename);
int export_results_to_json(const banner_collection_t *collection, const char *filename);

// Database functions (SQLite)
int init_banner_database(const char *db_file);
int save_result_to_database(const char *db_file, const banner_result_t *result);
int save_collection_to_database(const char *db_file, const banner_collection_t *collection);
int load_results_from_database(const char *db_file, banner_collection_t *collection, const char *host_filter);

// Bulk operations
int grab_banners_from_hosts_file(const char *hosts_file, banner_collection_t *collection, banner_config_t *config);
int grab_banners_from_scan_results(const char *scan_results_file, banner_collection_t *collection, banner_config_t *config);

// Display functions
void print_banner_result(const banner_result_t *result);
void print_banner_summary(const banner_collection_t *collection);
void print_service_statistics(const banner_collection_t *collection);

#endif // BANNER_GRABBER_H