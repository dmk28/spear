#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include <stdatomic.h>
#include <fcntl.h>
#define ACK_SCAN
#include "../scanners/syn/scanner.h"
#include "../scanners/ack/scanner.h"
#include "../scanners/util/packet_creation.h"
#include "../scanners/xmas/xmas.h"
#include "../utils/resolver.h"
#include "../utils/default_ports.h"
#include "../utils/banner_grabber.h"

#define DEFAULT_THREAD_NUM 10
#define INVALID_PORT -1
#define MAX_PORT 65535
#define MIN_PORT 1
#define MAX_THREADS 100
#define TIMEOUT_SECONDS 2
#define RAW_SCAN_TIMEOUT 5
#define MAX_THREAD_LOCAL_RESULTS 1000
#define RAW_SCAN_RETRIES 2

// Global quiet flag for raw socket scanners
int g_quiet_mode = 1;

// Scan types
typedef enum {
    SCAN_CONNECT = 0,
    SCAN_SYN,
    SCAN_ACK,
    SCAN_XMAS
} scan_type_t;

// Structure to handle optional port ranges
typedef struct {
    int start_port;
    int end_port;
    bool is_range;
    bool is_valid;
    bool is_default_list;
    port_list_type_t list_type;
} port_config_t;

typedef struct {
    char *ip;
    int *start_port;
    int *end_port;
    int *thread_count;
} scan_config_t;

// Structure to pass data to worker threads
typedef struct {
    char *target_ip;
    int port;
    int thread_id;
    int *open_ports;
    int *open_count;
    pthread_mutex_t *mutex;
    int grab_banners;
    banner_collection_t *banner_collection;
    banner_config_t *banner_config;
    scan_type_t scan_type;
} thread_data_t;

// Structure for work queue approach
typedef struct {
    int *ports;
    int total_ports;
    int current_index;
    char *target_ip;
    pthread_mutex_t *queue_mutex;
    int *open_ports;
    int *open_count;
    pthread_mutex_t *result_mutex;
    scan_type_t scan_type;
} work_queue_t;

// Structure for lock-free approach
typedef struct {
    int *ports;
    int total_ports;
    atomic_int *current_index;  // Pointer to shared atomic counter
    char *target_ip;
    int thread_id;
    scan_type_t scan_type;
    int verbose;
} lockfree_queue_t;

// Thread-local result structure
typedef struct {
    int open_ports[MAX_THREAD_LOCAL_RESULTS];
    int count;
    int thread_id;
} thread_results_t;

// Comparison function for sorting ports in numerical order
int port_compare(const void *a, const void *b) {
    int port_a = *(const int*)a;
    int port_b = *(const int*)b;
    return port_a - port_b;
}

int scan_port(const char *ip, int port, int timeout_sec);
// Function to parse port argument (single port, range, or default list)
port_config_t parse_port_argument(const char *port_str) {
    port_config_t config = {0};
    config.is_valid = false;
    config.is_range = false;
    config.is_default_list = false;

    if (!port_str || strlen(port_str) == 0) {
        return config; // Invalid - empty string
    }

    // Check if it's a default port list name
    port_list_type_t list_type = parse_port_list_name(port_str);
    if (list_type != -1) {
        config.is_default_list = true;
        config.list_type = list_type;
        config.is_valid = true;
        return config;
    }

    // Make a copy of the string since strtok modifies it
    char *port_copy = strdup(port_str);
    if (!port_copy) {
        return config; // Memory allocation failed
    }

    // Check if it contains a dash (range)
    char *dash_pos = strchr(port_copy, '-');

    if (dash_pos) {
        // It's a range: start-end
        *dash_pos = '\0'; // Split the string
        char *start_str = port_copy;
        char *end_str = dash_pos + 1;

        // Parse start port
        char *endptr;
        long start = strtol(start_str, &endptr, 10);
        if (*endptr != '\0' || start < MIN_PORT || start > MAX_PORT) {
            free(port_copy);
            return config; // Invalid start port
        }

        // Parse end port
        long end = strtol(end_str, &endptr, 10);
        if (*endptr != '\0' || end < MIN_PORT || end > MAX_PORT || end < start) {
            free(port_copy);
            return config; // Invalid end port or end < start
        }

        config.start_port = (int)start;
        config.end_port = (int)end;
        config.is_range = true;
        config.is_valid = true;
    } else {
        // Single port
        char *endptr;
        long port = strtol(port_copy, &endptr, 10);
        if (*endptr != '\0' || port < MIN_PORT || port > MAX_PORT) {
            free(port_copy);
            return config; // Invalid single port
        }

        config.start_port = (int)port;
        config.end_port = (int)port;
        config.is_range = false;
        config.is_valid = true;
    }

    free(port_copy);
    return config;
}



// Generic port scan function that calls the appropriate scanner
int scan_port_generic(const char *ip, int port, int timeout_sec, scan_type_t scan_type, int verbose) {
    int result;

    // Set quiet mode based on verbose flag
    g_quiet_mode = !verbose;

    switch (scan_type) {
        case SCAN_SYN:
            result = syn_scan(ip, port);
            if (result == 1) {
                return 1; // Definitely open (got SYN-ACK)
            } else if (result == 0) {
                return 0; // Definitely closed (got RST)
            } else {
                // No response - could be filtered or open
                // For stealth scanning, verify with connect scan
                return scan_port(ip, port, 1); // Quick connect test
            }
        case SCAN_ACK:
            result = ack_scan(ip, port);
            if (result == 1) {
                // Got ACK response - port is unfiltered, verify if open
                return scan_port(ip, port, timeout_sec);
            } else if (result == 0) {
                // Got RST - port is unfiltered but closed
                return 0;
            } else {
                // No response - likely filtered
                return 0;
            }
        case SCAN_XMAS:
            // XMAS scan - no response usually means open|filtered
            perform_random_xmas_scan(ip, port, 2);
            // XMAS interpretation: no RST = potentially open
            // Verify with connect scan for accuracy
            result = scan_port(ip, port, timeout_sec);
            return result;
        case SCAN_CONNECT:
        default:
            return scan_port(ip, port, timeout_sec);
    }
}

// Simple TCP connect scan for a single port
int scan_port(const char *ip, int port, int timeout_sec) {
    int sock;
    struct sockaddr_in target;
    struct timeval timeout;
    int result;

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return 0; // Failed to create socket
    }

    // Set timeout
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    // Set up target address
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    inet_pton(AF_INET, ip, &target.sin_addr);

    // Attempt connection
    result = connect(sock, (struct sockaddr*)&target, sizeof(target));
    close(sock);

    return (result == 0) ? 1 : 0; // 1 if open, 0 if closed
}

// Thread function for individual port scanning
void* scan_thread_individual(void *arg) {
    thread_data_t *data = (thread_data_t*)arg;

    // Add scan delay if specified (for rate limiting)
    if (data->scan_type != SCAN_CONNECT) {
        usleep(data->thread_id * 1000); // Stagger initial timing
    }

    int result = scan_port_generic(data->target_ip, data->port, TIMEOUT_SECONDS, data->scan_type, data->banner_config ? data->banner_config->verbose : 0);

    if (result == 1) { // Port is open
        pthread_mutex_lock(data->mutex);
        data->open_ports[(*data->open_count)++] = data->port;
        // Don't print here - we'll print in sorted order later

        pthread_mutex_unlock(data->mutex);
    }

    return NULL;
}

// Thread function for work queue approach
void* scan_thread_queue(void* arg) {
    work_queue_t *queue = (work_queue_t*)arg;
    int port;

    while (1) {
        // Get next port from queue
        pthread_mutex_lock(queue->queue_mutex);
        if (queue->current_index >= queue->total_ports) {
            pthread_mutex_unlock(queue->queue_mutex);
            break; // No more work
        }
        port = queue->ports[queue->current_index++];
        pthread_mutex_unlock(queue->queue_mutex);

        // Scan the port
        if (scan_port_generic(queue->target_ip, port, TIMEOUT_SECONDS, queue->scan_type, 0)) {
            pthread_mutex_lock(queue->result_mutex);
            queue->open_ports[(*queue->open_count)++] = port;
            printf("Port %d is OPEN\n", port);
            pthread_mutex_unlock(queue->result_mutex);
        }
    }

    return NULL;
}

// Method 1: One thread per port (suitable for small port ranges)
void scan_ports_individual_threads(const char *ip, int start_port, int end_port, int grab_banners, banner_collection_t *banner_collection, banner_config_t *banner_config, scan_type_t scan_type, int verbose) {
    int port_count = end_port - start_port + 1;
    pthread_t *threads = malloc(port_count * sizeof(pthread_t));
    thread_data_t *thread_data = malloc(port_count * sizeof(thread_data_t));
    int *open_ports = malloc(port_count * sizeof(int));
    int open_count = 0;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    // Scanning quietly - progress messages only in verbose mode
    if (verbose) printf("Starting individual thread scan of %d ports...\n", port_count);

    // Create threads
    for (int i = 0; i < port_count; i++) {
        thread_data[i].target_ip = (char*)ip;
        thread_data[i].port = start_port + i;
        thread_data[i].thread_id = i;
        thread_data[i].open_ports = open_ports;
        thread_data[i].open_count = &open_count;
        thread_data[i].mutex = &mutex;
        thread_data[i].grab_banners = grab_banners;
        thread_data[i].banner_collection = banner_collection;
        thread_data[i].banner_config = banner_config;
        thread_data[i].scan_type = scan_type;

        pthread_create(&threads[i], NULL, scan_thread_individual, &thread_data[i]);
    }

    // Wait for all threads to complete
    for (int i = 0; i < port_count; i++) {
        pthread_join(threads[i], NULL);
    }

    // Sort the open ports in numerical order
    if (open_count > 0) {
        qsort(open_ports, open_count, sizeof(int), port_compare);
        
        // Print results in order and grab banners
        for (int i = 0; i < open_count; i++) {
            int port = open_ports[i];
            printf("Port %d is OPEN\n", port);
            
            // Grab banner if enabled
            if (grab_banners && banner_collection && banner_config) {
                banner_result_t banner_result;
                if (grab_banner(ip, port, &banner_result, banner_config->timeout) == 0) {
                    add_banner_result(banner_collection, &banner_result);
                    if (banner_config->verbose) {
                        printf("    [+] Banner: %s\n", banner_result.service);
                    }
                }
            }
        }
    }

    if (verbose) {
        printf("Scan complete. Found %d open ports:\n", open_count);
        for (int i = 0; i < open_count; i++) {
            printf("  Port %d\n", open_ports[i]);
        }
    } else if (open_count == 0) {
        printf("No open ports found.\n");
    }

    free(threads);
    free(thread_data);
    free(open_ports);
    pthread_mutex_destroy(&mutex);
}

// Method 2: Thread pool with shared work queue (suitable for medium port ranges)
void scan_ports_thread_pool(const char *ip, int start_port, int end_port, int num_threads, int verbose) {
    int port_count = end_port - start_port + 1;
    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    int *ports = malloc(port_count * sizeof(int));
    int *open_ports = malloc(port_count * sizeof(int));
    int open_count = 0;

    // Initialize work queue
    work_queue_t queue = {0};
    queue.ports = ports;
    queue.total_ports = port_count;
    queue.current_index = 0;
    queue.target_ip = (char*)ip;
    queue.open_ports = open_ports;
    queue.open_count = &open_count;

    // Initialize mutexes
    pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;
    queue.queue_mutex = &queue_mutex;
    queue.result_mutex = &result_mutex;

    // Fill port array
    for (int i = 0; i < port_count; i++) {
        ports[i] = start_port + i;
    }

    if (verbose) printf("Starting thread pool scan with %d threads for %d ports...\n", num_threads, port_count);

    // Create worker threads
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, scan_thread_queue, &queue);
    }

    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    if (verbose) {
        printf("Scan complete. Found %d open ports:\n", open_count);
        for (int i = 0; i < open_count; i++) {
            printf("  Port %d\n", open_ports[i]);
        }
    } else if (open_count == 0) {
        printf("No open ports found.\n");
    }

    free(threads);
    free(ports);
    free(open_ports);
    pthread_mutex_destroy(&queue_mutex);
    pthread_mutex_destroy(&result_mutex);
}

// Lock-free thread function using atomic operations
void* scan_thread_lockfree(void* arg) {
    lockfree_queue_t *queue = (lockfree_queue_t*)arg;

    // Thread-local results storage
    static __thread thread_results_t local_results = {0};
    local_results.thread_id = queue->thread_id;
    local_results.count = 0;

    int port_index;
    while ((port_index = atomic_fetch_add(queue->current_index, 1)) < queue->total_ports) {
        int port = queue->ports[port_index];

        if (scan_port_generic(queue->target_ip, port, TIMEOUT_SECONDS, queue->scan_type, queue->verbose)) {
            if (local_results.count < MAX_THREAD_LOCAL_RESULTS) {
                local_results.open_ports[local_results.count++] = port;
            }
        }
    }

    // Return our local results
    thread_results_t *results = malloc(sizeof(thread_results_t));
    *results = local_results;
    return results;
}

void scan_ports_lockfree(const char *ip, int start_port, int end_port, int num_threads, int grab_banners, banner_collection_t *banner_collection, banner_config_t *banner_config, scan_type_t scan_type, int verbose) {
    int port_count = end_port - start_port + 1;
    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    int *ports = malloc(port_count * sizeof(int));
    lockfree_queue_t *queues = malloc(num_threads * sizeof(lockfree_queue_t));

    // Fill port array
    for (int i = 0; i < port_count; i++) {
        ports[i] = start_port + i;
    }

    // Initialize shared atomic counter
    atomic_int shared_index = ATOMIC_VAR_INIT(0);

    if (verbose) printf("Starting lock-free scan with %d threads for %d ports...\n", num_threads, port_count);

    // Create worker threads - all share the same atomic counter
    for (int i = 0; i < num_threads; i++) {
        queues[i].ports = ports;
        queues[i].total_ports = port_count;
        queues[i].current_index = &shared_index;
        queues[i].target_ip = (char*)ip;
        queues[i].thread_id = i;
        queues[i].scan_type = scan_type;
        queues[i].verbose = verbose;

        pthread_create(&threads[i], NULL, scan_thread_lockfree, &queues[i]);
    }

    // Collect all results from all threads first
    int *all_open_ports = malloc(port_count * sizeof(int));
    int total_open = 0;

    for (int i = 0; i < num_threads; i++) {
        thread_results_t *results;
        pthread_join(threads[i], (void**)&results);

        if (results) {
            // Add this thread's results to the master list
            for (int j = 0; j < results->count; j++) {
                all_open_ports[total_open++] = results->open_ports[j];
            }
            free(results);
        }
    }

    // Sort all open ports numerically
    if (total_open > 0) {
        qsort(all_open_ports, total_open, sizeof(int), port_compare);
        
        // Print results in sorted order and grab banners
        for (int i = 0; i < total_open; i++) {
            int port = all_open_ports[i];
            printf("Port %d is OPEN\n", port);

            // Grab banner if enabled
            if (grab_banners && banner_collection && banner_config) {
                banner_result_t banner_result;
                if (grab_banner(ip, port, &banner_result, banner_config->timeout) == 0) {
                    add_banner_result(banner_collection, &banner_result);
                    if (banner_config->verbose) {
                        printf("    [+] Banner: %s\n", banner_result.service);
                    }
                }
            }
        }
    }

    if (verbose) {
        printf("Lock-free scan complete. Found %d total open ports.\n", total_open);
    } else if (total_open == 0) {
        printf("No open ports found.\n");
    }

    free(all_open_ports);
    free(threads);
    free(ports);
    free(queues);
}

// Method 4: Scan from predefined port array
void scan_ports_from_array(const char *ip, const int *port_array, int port_count, int num_threads, int grab_banners, banner_collection_t *banner_collection, banner_config_t *banner_config, scan_type_t scan_type, int verbose) {
    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    lockfree_queue_t *queues = malloc(num_threads * sizeof(lockfree_queue_t));

    // Initialize shared atomic counter
    atomic_int shared_index = ATOMIC_VAR_INIT(0);

    if (verbose) printf("Starting array-based scan with %d threads for %d predefined ports...\n", num_threads, port_count);

    // Create worker threads - all share the same atomic counter and port array
    for (int i = 0; i < num_threads; i++) {
        queues[i].ports = (int*)port_array;  // Cast away const for compatibility
        queues[i].total_ports = port_count;
        queues[i].current_index = &shared_index;
        queues[i].target_ip = (char*)ip;
        queues[i].thread_id = i;
        queues[i].scan_type = scan_type;
        queues[i].verbose = verbose;

        pthread_create(&threads[i], NULL, scan_thread_lockfree, &queues[i]);
    }

    // Collect all results from all threads first
    int *all_open_ports = malloc(port_count * sizeof(int));
    int total_open = 0;

    for (int i = 0; i < num_threads; i++) {
        thread_results_t *results;
        pthread_join(threads[i], (void**)&results);

        if (results) {
            // Add this thread's results to the master list
            for (int j = 0; j < results->count; j++) {
                all_open_ports[total_open++] = results->open_ports[j];
            }
            free(results);
        }
    }

    // Sort all open ports numerically
    if (total_open > 0) {
        qsort(all_open_ports, total_open, sizeof(int), port_compare);
        
        // Print results in sorted order and grab banners
        for (int i = 0; i < total_open; i++) {
            int port = all_open_ports[i];
            printf("Port %d is OPEN\n", port);

            // Grab banner if enabled
            if (grab_banners && banner_collection && banner_config) {
                banner_result_t banner_result;
                if (grab_banner(ip, port, &banner_result, banner_config->timeout) == 0) {
                    add_banner_result(banner_collection, &banner_result);
                    if (banner_config->verbose) {
                        printf("    [+] Banner: %s\n", banner_result.service);
                    }
                }
            }
        }
    }

    if (verbose) {
        printf("Array scan complete. Found %d total open ports.\n", total_open);
    } else if (total_open == 0) {
        printf("No open ports found.\n");
    }

    free(all_open_ports);
    free(threads);
    free(queues);
}

int main(int argc, char *argv[]) {
    // Optional variables with default values
    int threads = DEFAULT_THREAD_NUM; // Default value
    char *target_ip = NULL;
    port_config_t port_config = {0};
    int grab_banners = 0;
    int banner_timeout = BANNER_TIMEOUT;
    char *output_file = NULL;
    char *db_file = NULL;
    int verbose = 0;
    scan_type_t scan_type = SCAN_CONNECT;
    int scan_delay = 0;

    // Parse command line flags
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--banner") == 0) {
            grab_banners = 1;
        } else if (strcmp(argv[i], "-sT") == 0 || strcmp(argv[i], "--tcp-connect") == 0) {
            scan_type = SCAN_CONNECT;
        } else if (strcmp(argv[i], "-sS") == 0 || strcmp(argv[i], "--syn") == 0) {
            scan_type = SCAN_SYN;
        } else if (strcmp(argv[i], "-sA") == 0 || strcmp(argv[i], "--ack") == 0) {
            scan_type = SCAN_ACK;
        } else if (strcmp(argv[i], "-sX") == 0 || strcmp(argv[i], "--xmas") == 0) {
            scan_type = SCAN_XMAS;
        } else if (strcmp(argv[i], "--scan-delay") == 0) {
            if (i + 1 < argc) {
                scan_delay = atoi(argv[i + 1]);
                i++; // Skip next argument
            }
        } else if (strcmp(argv[i], "-bt") == 0 || strcmp(argv[i], "--banner-timeout") == 0) {
            if (i + 1 < argc) {
                banner_timeout = atoi(argv[i + 1]);
                i++; // Skip next argument
            }
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) {
                output_file = argv[i + 1];
                i++; // Skip next argument
            }
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--database") == 0) {
            if (i + 1 < argc) {
                db_file = argv[i + 1];
                i++; // Skip next argument
            }
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("SPEAR - PORT SCANNER\n");
            printf("Usage: %s [OPTIONS] <target_ip_or_hostname> <port_specification> [threads]\n", argv[0]);
            printf("\nScan Options:\n");
            printf("  -sT, --tcp-connect          TCP Connect scan (default)\n");
            printf("  -sS, --syn                  SYN scan (requires root)\n");
            printf("  -sA, --ack                  ACK scan (requires root)\n");
            printf("  -sX, --xmas                 XMAS scan (requires root)\n");
            printf("  --scan-delay MS             Delay between scans in milliseconds\n");
            printf("\nBanner Options:\n");
            printf("  -b, --banner                Enable banner grabbing\n");
            printf("  -bt, --banner-timeout SEC   Banner grab timeout (default: %d)\n", BANNER_TIMEOUT);
            printf("\nOutput Options:\n");
            printf("  -o, --output FILE           Export banner results to text file\n");
            printf("  -d, --database FILE         Save banner results to SQLite database\n");
            printf("  -v, --verbose               Enable verbose output\n");
            printf("  -h, --help                  Show this help message\n");
            printf("\nPort specification can be:\n");
            printf("  - Single port: 80\n");
            printf("  - Port range: 80-443\n");
            printf("  - Default port list: quick, top10, top50, top100, top1000\n");
            printf("\nExamples:\n");
            printf("  %s 192.168.1.1 80\n", argv[0]);
            printf("  %s google.com 80-443\n", argv[0]);
            printf("  %s -sS -b -o banners.txt scanme.nmap.org top100 20\n", argv[0]);
            printf("  %s -sA -b -d results.db github.com quick\n", argv[0]);
            printf("  %s -sX scanme.nmap.org 80-443\n", argv[0]);
            printf("\n");
            print_available_port_lists();
            return 0;
        }
    }

    // Find non-flag arguments
    int arg_count = 0;
    char *args[4];
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-' && arg_count < 4) {
            // Check if this is not a flag value
            // Skip next argument
            if (i == 1 || argv[i-1][0] != '-' ||
                (strcmp(argv[i-1], "-bt") != 0 && strcmp(argv[i-1], "--banner-timeout") != 0 &&
                 strcmp(argv[i-1], "-o") != 0 && strcmp(argv[i-1], "--output") != 0 &&
                 strcmp(argv[i-1], "-d") != 0 && strcmp(argv[i-1], "--database") != 0 &&
                 strcmp(argv[i-1], "--scan-delay") != 0)) {
                args[arg_count++] = argv[i];
            }
        }
    }

    if (arg_count < 2) {
        printf("Usage: %s [OPTIONS] <target_ip_or_hostname> <port_specification> [threads]\n", argv[0]);
        printf("Use --help for full help message\n");
        return 1;
    }

    // Resolve hostname to IP if necessary
    char *resolved_ip = resolve_hostname(args[0]);
    if (!resolved_ip) {
        printf("Error: Could not resolve '%s' to an IP address\n", args[0]);
        return 1;
    }
    target_ip = resolved_ip;

    // Parse port argument
    port_config = parse_port_argument(args[1]);
    if (!port_config.is_valid) {
        printf("Error: Invalid port specification '%s'\n", args[1]);
        printf("Port must be a number between %d and %d, or a range like '80-443'\n",
               MIN_PORT, MAX_PORT);
        return 1;
    }

    // Optional third argument: number of threads
    if (arg_count >= 3) {
        char *endptr;
        long thread_count = strtol(args[2], &endptr, 10);
        if (*endptr == '\0' && thread_count > 0 && thread_count <= MAX_THREADS) {
            threads = (int)thread_count;
        } else {
            printf("Warning: Invalid thread count '%s' (must be 1-%d), using default (%d)\n",
                   args[2], MAX_THREADS, DEFAULT_THREAD_NUM);
        }
    }

    // Initialize banner grabbing if enabled
    banner_collection_t *banner_collection = NULL;
    banner_config_t *banner_config = NULL;

    if (grab_banners) {
        banner_collection = create_banner_collection(1000);
        banner_config = create_banner_config();

        if (!banner_collection || !banner_config) {
            printf("Error: Failed to initialize banner grabbing\n");
            return 1;
        }

        banner_config->timeout = banner_timeout;
        banner_config->verbose = verbose;
        printf("Banner grabbing enabled (timeout: %ds)\n", banner_timeout);
    }

    // Check for root privileges if needed
    const char* scan_names[] = {"Connect", "SYN", "ACK", "XMAS"};
    if (scan_type != SCAN_CONNECT && getuid() != 0) {
        printf("Warning: %s scan requires root privileges. Falling back to Connect scan.\n", scan_names[scan_type]);
        scan_type = SCAN_CONNECT;
    }

    // Set global quiet mode
    g_quiet_mode = !verbose;

    // Warn about raw socket scan limitations
    if (scan_type != SCAN_CONNECT && verbose) {
        printf("Note: Raw socket scans use hybrid verification for better accuracy.\n");
        if (scan_delay > 0) {
            printf("Scan delay: %dms between probes\n", scan_delay);
        }
    }

    // Display parsed configuration
    if (verbose) {
        printf("Target IP: %s\n", target_ip);
        printf("Scan Type: %s\n", scan_names[scan_type]);
    }

    if (port_config.is_default_list) {
        port_list_info_t port_list = get_port_list(port_config.list_type);
        if (verbose) {
            printf("Using port list: %s (%d ports)\n", port_list.description, port_list.count);
            printf("Threads: %d\n", threads);
            printf("Starting scan...\n");
        }
        // Scanning quietly - only show open ports

        // Use lock-free approach for default port lists (they're typically large)
        scan_ports_from_array(target_ip, port_list.ports, port_list.count, threads, grab_banners, banner_collection, banner_config, scan_type, verbose);
    } else {
        if (verbose) {
            if (port_config.is_range) {
                printf("Port range: %d-%d\n", port_config.start_port, port_config.end_port);
            } else {
                printf("Single port: %d\n", port_config.start_port);
            }
            printf("Threads: %d\n", threads);
            printf("Starting scan...\n");
        }
        // Scanning quietly - only show open ports

        // Choose threading approach based on port range size
        int total_ports = port_config.end_port - port_config.start_port + 1;

        if (total_ports <= MAX_THREADS) {
            // Small range: one thread per port
            scan_ports_individual_threads(target_ip, port_config.start_port, port_config.end_port, grab_banners, banner_collection, banner_config, scan_type, verbose);
        } else {
            // Large range: use lock-free approach for better performance
            scan_ports_lockfree(target_ip, port_config.start_port, port_config.end_port, threads, grab_banners, banner_collection, banner_config, scan_type, verbose);
        }
    }

    if (verbose) printf("Scan completed!\n");

    // Handle banner results if banner grabbing was enabled
    if (grab_banners && banner_collection && banner_collection->count > 0) {
        if (verbose) {
            printf("\nBanner Summary:\n");
            print_banner_summary(banner_collection);
        }

        // Export results if requested
        if (output_file) {
            if (verbose) printf("Exporting banner results to: %s\n", output_file);
            if (export_results_to_text(banner_collection, output_file) == 0) {
                if (verbose) printf("Banner results exported successfully\n");
            } else {
                printf("Failed to export banner results\n");
            }
        }

        if (db_file) {
            if (verbose) printf("Saving banner results to database: %s\n", db_file);
            if (save_collection_to_database(db_file, banner_collection) == 0) {
                if (verbose) printf("Banner results saved to database successfully\n");
            } else {
                printf("Failed to save banner results to database\n");
            }
        }
    }

    // Cleanup
    if (banner_collection) free_banner_collection(banner_collection);
    if (banner_config) free_banner_config(banner_config);

    return 0;
}
