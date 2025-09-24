#include "banner_grabber.h"
#include <fcntl.h>
#include <sys/select.h>
#include <signal.h>
#include <sqlite3.h>
#include <ctype.h>

// Service probes for different protocols
static const char *HTTP_PROBE = "GET / HTTP/1.1\r\nHost: %s\r\nUser-Agent: SpearBanner/1.0\r\nConnection: close\r\n\r\n";
static const char *FTP_PROBE = "";
static const char *SSH_PROBE = "";
static const char *SMTP_PROBE = "EHLO spear.local\r\n";
static const char *TELNET_PROBE = "";
static const char *POP3_PROBE = "USER test\r\n";
static const char *IMAP_PROBE = "a001 CAPABILITY\r\n";

// Common HTTP ports
static int http_ports[] = {80, 8080, 8000, 8008, 8888, 443, 8443, 9000, 9001, -1};
static int ssl_ports[] = {443, 8443, 993, 995, 636, 465, 587, -1};

// Create banner configuration with defaults
banner_config_t* create_banner_config(void) {
    banner_config_t *config = malloc(sizeof(banner_config_t));
    if (!config) return NULL;
    
    config->timeout = BANNER_TIMEOUT;
    config->verbose = 0;
    config->output_file = NULL;
    config->db_file = NULL;
    config->max_results = MAX_RESULTS;
    
    return config;
}

void free_banner_config(banner_config_t *config) {
    if (!config) return;
    
    if (config->output_file) free(config->output_file);
    if (config->db_file) free(config->db_file);
    free(config);
}

// Create banner collection
banner_collection_t* create_banner_collection(int initial_capacity) {
    banner_collection_t *collection = malloc(sizeof(banner_collection_t));
    if (!collection) return NULL;
    
    collection->results = malloc(sizeof(banner_result_t) * initial_capacity);
    if (!collection->results) {
        free(collection);
        return NULL;
    }
    
    collection->count = 0;
    collection->capacity = initial_capacity;
    
    return collection;
}

void free_banner_collection(banner_collection_t *collection) {
    if (!collection) return;
    
    if (collection->results) free(collection->results);
    free(collection);
}

int add_banner_result(banner_collection_t *collection, const banner_result_t *result) {
    if (!collection || !result) return -1;
    
    // Expand capacity if needed
    if (collection->count >= collection->capacity) {
        int new_capacity = collection->capacity * 2;
        banner_result_t *new_results = realloc(collection->results, 
                                              sizeof(banner_result_t) * new_capacity);
        if (!new_results) return -1;
        
        collection->results = new_results;
        collection->capacity = new_capacity;
    }
    
    // Copy result
    memcpy(&collection->results[collection->count], result, sizeof(banner_result_t));
    collection->count++;
    
    return 0;
}

// Connect with timeout
int connect_with_timeout(const char *host, int port, int timeout) {
    int sockfd;
    struct sockaddr_in server_addr;
    struct hostent *host_entry;
    struct timeval tv;
    fd_set writefds;
    int flags, result;
    socklen_t len;
    int error;
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;
    
    // Set non-blocking
    flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    
    // Resolve hostname
    host_entry = gethostbyname(host);
    if (!host_entry) {
        close(sockfd);
        return -1;
    }
    
    // Setup address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr, host_entry->h_addr, host_entry->h_length);
    
    // Attempt connection
    result = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    if (result < 0 && errno != EINPROGRESS) {
        close(sockfd);
        return -1;
    }
    
    if (result == 0) {
        // Connection successful immediately
        fcntl(sockfd, F_SETFL, flags); // Restore blocking mode
        return sockfd;
    }
    
    // Wait for connection with timeout
    FD_ZERO(&writefds);
    FD_SET(sockfd, &writefds);
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    
    result = select(sockfd + 1, NULL, &writefds, NULL, &tv);
    
    if (result <= 0) {
        close(sockfd);
        return -1; // Timeout or error
    }
    
    // Check if connection succeeded
    len = sizeof(error);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
        close(sockfd);
        return -1;
    }
    
    // Restore blocking mode
    fcntl(sockfd, F_SETFL, flags);
    
    return sockfd;
}

// Send probe and read response
int send_probe_and_read(int sockfd, const char *probe, char *buffer, int buffer_size, int timeout) {
    struct timeval tv;
    fd_set readfds;
    int bytes_read = 0, total_read = 0;
    
    // Set socket timeout
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    // Send probe if provided
    if (probe && strlen(probe) > 0) {
        if (send(sockfd, probe, strlen(probe), 0) < 0) {
            return -1;
        }
    }
    
    // Read response
    memset(buffer, 0, buffer_size);
    
    while (total_read < buffer_size - 1) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        
        int select_result = select(sockfd + 1, &readfds, NULL, NULL, &tv);
        if (select_result <= 0) break; // Timeout or error
        
        bytes_read = recv(sockfd, buffer + total_read, buffer_size - total_read - 1, 0);
        if (bytes_read <= 0) break;
        
        total_read += bytes_read;
        
        // Break if we get a complete response (heuristic)
        if (strstr(buffer, "\r\n\r\n") || strstr(buffer, "\n\n")) {
            break;
        }
    }
    
    buffer[total_read] = '\0';
    return total_read;
}

// Clean banner text by removing unprintable characters
void clean_banner_text(char *banner) {
    char *src = banner, *dst = banner;
    
    while (*src) {
        if (isprint(*src) || *src == '\n' || *src == '\r' || *src == '\t') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
    
    // Remove trailing whitespace
    int len = strlen(banner);
    while (len > 0 && isspace(banner[len - 1])) {
        banner[--len] = '\0';
    }
}

// Service detection based on port
const char* detect_service(int port) {
    switch (port) {
        case 21: return "ftp";
        case 22: return "ssh";
        case 23: return "telnet";
        case 25: return "smtp";
        case 53: return "dns";
        case 80: return "http";
        case 110: return "pop3";
        case 143: return "imap";
        case 443: return "https";
        case 993: return "imaps";
        case 995: return "pop3s";
        case 8080: case 8000: case 8008: return "http-alt";
        case 8443: return "https-alt";
        default: return "unknown";
    }
}

int is_http_port(int port) {
    for (int i = 0; http_ports[i] != -1; i++) {
        if (http_ports[i] == port) return 1;
    }
    return 0;
}

int is_ssl_port(int port) {
    for (int i = 0; ssl_ports[i] != -1; i++) {
        if (ssl_ports[i] == port) return 1;
    }
    return 0;
}

// Generic banner grabber
int grab_banner(const char *host, int port, banner_result_t *result, int timeout) {
    const char *service = detect_service(port);
    
    // Use service-specific grabber if available
    if (strcmp(service, "http") == 0 || strcmp(service, "http-alt") == 0) {
        return grab_http_banner(host, port, result, timeout);
    } else if (strcmp(service, "ftp") == 0) {
        return grab_ftp_banner(host, port, result, timeout);
    } else if (strcmp(service, "ssh") == 0) {
        return grab_ssh_banner(host, port, result, timeout);
    } else if (strcmp(service, "smtp") == 0) {
        return grab_smtp_banner(host, port, result, timeout);
    } else {
        // Generic grab - just connect and read
        return grab_banner_with_probe(host, port, "", result, timeout);
    }
}

// Generic banner grabber with custom probe
int grab_banner_with_probe(const char *host, int port, const char *probe, banner_result_t *result, int timeout) {
    int sockfd;
    char buffer[MAX_BANNER_SIZE];
    int bytes_read;
    
    // Initialize result
    memset(result, 0, sizeof(banner_result_t));
    strncpy(result->host, host, MAX_HOST_SIZE - 1);
    result->port = port;
    strncpy(result->service, detect_service(port), MAX_SERVICE_SIZE - 1);
    result->timestamp = time(NULL);
    result->success = 0;
    
    // Connect to target
    sockfd = connect_with_timeout(host, port, timeout);
    if (sockfd < 0) {
        snprintf(result->banner, MAX_BANNER_SIZE, "Connection failed");
        return -1;
    }
    
    // Send probe and read response
    bytes_read = send_probe_and_read(sockfd, probe, buffer, sizeof(buffer), timeout);
    close(sockfd);
    
    if (bytes_read > 0) {
        strncpy(result->banner, buffer, MAX_BANNER_SIZE - 1);
        clean_banner_text(result->banner);
        result->success = 1;
        return 0;
    } else {
        snprintf(result->banner, MAX_BANNER_SIZE, "No response received");
        return -1;
    }
}

// HTTP banner grabber
int grab_http_banner(const char *host, int port, banner_result_t *result, int timeout) {
    char probe[512];
    snprintf(probe, sizeof(probe), HTTP_PROBE, host);
    return grab_banner_with_probe(host, port, probe, result, timeout);
}

// FTP banner grabber
int grab_ftp_banner(const char *host, int port, banner_result_t *result, int timeout) {
    return grab_banner_with_probe(host, port, FTP_PROBE, result, timeout);
}

// SSH banner grabber
int grab_ssh_banner(const char *host, int port, banner_result_t *result, int timeout) {
    return grab_banner_with_probe(host, port, SSH_PROBE, result, timeout);
}

// SMTP banner grabber
int grab_smtp_banner(const char *host, int port, banner_result_t *result, int timeout) {
    return grab_banner_with_probe(host, port, SMTP_PROBE, result, timeout);
}

// Telnet banner grabber
int grab_telnet_banner(const char *host, int port, banner_result_t *result, int timeout) {
    return grab_banner_with_probe(host, port, TELNET_PROBE, result, timeout);
}

// POP3 banner grabber
int grab_pop3_banner(const char *host, int port, banner_result_t *result, int timeout) {
    return grab_banner_with_probe(host, port, POP3_PROBE, result, timeout);
}

// IMAP banner grabber
int grab_imap_banner(const char *host, int port, banner_result_t *result, int timeout) {
    return grab_banner_with_probe(host, port, IMAP_PROBE, result, timeout);
}

// Export functions
int export_results_to_text(const banner_collection_t *collection, const char *filename) {
    if (!collection || !filename) return -1;
    
    FILE *file = fopen(filename, "w");
    if (!file) return -1;
    
    fprintf(file, "Banner Grab Results\n");
    fprintf(file, "===================\n");
    fprintf(file, "Generated: %s\n", ctime(&(time_t){time(NULL)}));
    fprintf(file, "Total results: %d\n\n", collection->count);
    
    for (int i = 0; i < collection->count; i++) {
        const banner_result_t *result = &collection->results[i];
        fprintf(file, "Host: %s:%d (%s)\n", result->host, result->port, result->service);
        fprintf(file, "Timestamp: %s", ctime(&result->timestamp));
        fprintf(file, "Status: %s\n", result->success ? "Success" : "Failed");
        fprintf(file, "Banner:\n%s\n", result->banner);
        fprintf(file, "----------------------------------------\n\n");
    }
    
    fclose(file);
    return 0;
}

int export_results_to_csv(const banner_collection_t *collection, const char *filename) {
    if (!collection || !filename) return -1;
    
    FILE *file = fopen(filename, "w");
    if (!file) return -1;
    
    // Write CSV header
    fprintf(file, "host,port,service,timestamp,status,banner\n");
    
    for (int i = 0; i < collection->count; i++) {
        const banner_result_t *result = &collection->results[i];
        
        // Escape banner text for CSV
        char escaped_banner[MAX_BANNER_SIZE * 2];
        char *src = result->banner, *dst = escaped_banner;
        
        while (*src && dst < escaped_banner + sizeof(escaped_banner) - 2) {
            if (*src == '"') {
                *dst++ = '"';
                *dst++ = '"';
            } else if (*src == '\n' || *src == '\r') {
                *dst++ = ' ';
            } else {
                *dst++ = *src;
            }
            src++;
        }
        *dst = '\0';
        
        fprintf(file, "%s,%d,%s,%ld,%s,\"%s\"\n",
                result->host, result->port, result->service,
                result->timestamp, result->success ? "success" : "failed",
                escaped_banner);
    }
    
    fclose(file);
    return 0;
}

int export_results_to_json(const banner_collection_t *collection, const char *filename) {
    if (!collection || !filename) return -1;
    
    FILE *file = fopen(filename, "w");
    if (!file) return -1;
    
    fprintf(file, "{\n");
    fprintf(file, "  \"scan_info\": {\n");
    fprintf(file, "    \"timestamp\": %ld,\n", time(NULL));
    fprintf(file, "    \"total_results\": %d\n", collection->count);
    fprintf(file, "  },\n");
    fprintf(file, "  \"results\": [\n");
    
    for (int i = 0; i < collection->count; i++) {
        const banner_result_t *result = &collection->results[i];
        
        // Escape banner text for JSON
        char escaped_banner[MAX_BANNER_SIZE * 2];
        char *src = result->banner, *dst = escaped_banner;
        
        while (*src && dst < escaped_banner + sizeof(escaped_banner) - 2) {
            if (*src == '"') {
                *dst++ = '\\';
                *dst++ = '"';
            } else if (*src == '\\') {
                *dst++ = '\\';
                *dst++ = '\\';
            } else if (*src == '\n') {
                *dst++ = '\\';
                *dst++ = 'n';
            } else if (*src == '\r') {
                *dst++ = '\\';
                *dst++ = 'r';
            } else if (*src == '\t') {
                *dst++ = '\\';
                *dst++ = 't';
            } else {
                *dst++ = *src;
            }
            src++;
        }
        *dst = '\0';
        
        fprintf(file, "    {\n");
        fprintf(file, "      \"host\": \"%s\",\n", result->host);
        fprintf(file, "      \"port\": %d,\n", result->port);
        fprintf(file, "      \"service\": \"%s\",\n", result->service);
        fprintf(file, "      \"timestamp\": %ld,\n", result->timestamp);
        fprintf(file, "      \"success\": %s,\n", result->success ? "true" : "false");
        fprintf(file, "      \"banner\": \"%s\"\n", escaped_banner);
        fprintf(file, "    }%s\n", (i < collection->count - 1) ? "," : "");
    }
    
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");
    
    fclose(file);
    return 0;
}

// Database functions (SQLite)
int init_banner_database(const char *db_file) {
    sqlite3 *db;
    char *err_msg = 0;
    int rc;
    
    rc = sqlite3_open(db_file, &db);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        return -1;
    }
    
    const char *sql = "CREATE TABLE IF NOT EXISTS banners ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                     "host TEXT NOT NULL,"
                     "port INTEGER NOT NULL,"
                     "service TEXT,"
                     "banner TEXT,"
                     "timestamp INTEGER,"
                     "success INTEGER"
                     ");";
    
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }
    
    sqlite3_close(db);
    return 0;
}

int save_result_to_database(const char *db_file, const banner_result_t *result) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc;
    
    rc = sqlite3_open(db_file, &db);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        return -1;
    }
    
    const char *sql = "INSERT INTO banners (host, port, service, banner, timestamp, success) "
                     "VALUES (?, ?, ?, ?, ?, ?);";
    
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, result->host, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, result->port);
    sqlite3_bind_text(stmt, 3, result->service, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, result->banner, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 5, result->timestamp);
    sqlite3_bind_int(stmt, 6, result->success);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int save_collection_to_database(const char *db_file, const banner_collection_t *collection) {
    if (!collection || !db_file) return -1;
    
    // Initialize database
    if (init_banner_database(db_file) != 0) return -1;
    
    // Save each result
    for (int i = 0; i < collection->count; i++) {
        if (save_result_to_database(db_file, &collection->results[i]) != 0) {
            return -1;
        }
    }
    
    return 0;
}

// Display functions
void print_banner_result(const banner_result_t *result) {
    printf("Host: %s:%d (%s)\n", result->host, result->port, result->service);
    printf("Timestamp: %s", ctime(&result->timestamp));
    printf("Status: %s\n", result->success ? "Success" : "Failed");
    printf("Banner:\n%s\n", result->banner);
    printf("----------------------------------------\n");
}

void print_banner_summary(const banner_collection_t *collection) {
    if (!collection) return;
    
    int successful = 0;
    for (int i = 0; i < collection->count; i++) {
        if (collection->results[i].success) successful++;
    }
    
    printf("Banner Grab Summary\n");
    printf("==================\n");
    printf("Total attempts: %d\n", collection->count);
    printf("Successful: %d\n", successful);
    printf("Failed: %d\n", collection->count - successful);
    printf("Success rate: %.1f%%\n", 
           collection->count > 0 ? (float)successful / collection->count * 100 : 0);
}

void print_service_statistics(const banner_collection_t *collection) {
    if (!collection) return;
    
    // Simple service counting (could be improved with a hash table)
    printf("\nService Statistics\n");
    printf("==================\n");
    
    const char *services[] = {"http", "https", "ssh", "ftp", "smtp", "telnet", "pop3", "imap", "unknown", NULL};
    
    for (int i = 0; services[i]; i++) {
        int count = 0;
        for (int j = 0; j < collection->count; j++) {
            if (strcmp(collection->results[j].service, services[i]) == 0) {
                count++;
            }
        }
        if (count > 0) {
            printf("%s: %d\n", services[i], count);
        }
    }
}

// Utility function to format banner for display
void format_banner_for_display(const banner_result_t *result, char *output, int output_size) {
    snprintf(output, output_size, "[%s:%d] %s - %s", 
             result->host, result->port, result->service,
             result->success ? "SUCCESS" : "FAILED");
}