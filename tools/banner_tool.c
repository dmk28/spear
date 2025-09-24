#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include "../utils/banner_grabber.h"

#define MAX_THREADS 50
#define DEFAULT_THREADS 10
#define MAX_TARGETS 10000

typedef struct {
    char host[MAX_HOST_SIZE];
    int port;
} target_t;

typedef struct {
    target_t *targets;
    int target_count;
    int current_index;
    banner_collection_t *collection;
    banner_config_t *config;
    pthread_mutex_t mutex;
} thread_data_t;

// Global variables
static int interrupted = 0;
static thread_data_t thread_data;

// Signal handler
void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        interrupted = 1;
        printf("\n[!] Interrupt received, finishing current operations...\n");
    }
}

// Worker thread function
void* banner_worker(void* arg) {
    int thread_id = *(int*)arg;
    banner_result_t result;
    int local_index;
    
    while (!interrupted) {
        // Get next target
        pthread_mutex_lock(&thread_data.mutex);
        if (thread_data.current_index >= thread_data.target_count) {
            pthread_mutex_unlock(&thread_data.mutex);
            break;
        }
        local_index = thread_data.current_index++;
        pthread_mutex_unlock(&thread_data.mutex);
        
        target_t *target = &thread_data.targets[local_index];
        
        if (thread_data.config->verbose) {
            printf("[Thread %d] Grabbing banner from %s:%d\n", 
                   thread_id, target->host, target->port);
        }
        
        // Grab banner
        int success = grab_banner(target->host, target->port, &result, 
                                 thread_data.config->timeout);
        
        // Add result to collection
        pthread_mutex_lock(&thread_data.mutex);
        add_banner_result(thread_data.collection, &result);
        pthread_mutex_unlock(&thread_data.mutex);
        
        if (thread_data.config->verbose || success == 0) {
            printf("[%s] %s:%d (%s) - %s\n", 
                   success == 0 ? "+" : "-",
                   target->host, target->port, result.service,
                   success == 0 ? "Banner grabbed" : "Failed");
        }
    }
    
    return NULL;
}

// Parse targets from file
int load_targets_from_file(const char* filename, target_t* targets, int max_targets) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open targets file '%s'\n", filename);
        return -1;
    }
    
    char line[512];
    int count = 0;
    
    while (fgets(line, sizeof(line), file) && count < max_targets) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Skip empty lines and comments
        if (strlen(line) == 0 || line[0] == '#') continue;
        
        // Parse host:port
        char* colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            strncpy(targets[count].host, line, MAX_HOST_SIZE - 1);
            targets[count].port = atoi(colon + 1);
            
            if (targets[count].port > 0 && targets[count].port <= 65535) {
                count++;
            }
        } else {
            // Assume common ports if none specified
            strncpy(targets[count].host, line, MAX_HOST_SIZE - 1);
            
            // Add common service ports
            int common_ports[] = {21, 22, 23, 25, 53, 80, 110, 143, 443, 993, 995, 8080, -1};
            for (int i = 0; common_ports[i] != -1 && count < max_targets; i++) {
                strncpy(targets[count].host, line, MAX_HOST_SIZE - 1);
                targets[count].port = common_ports[i];
                count++;
            }
        }
    }
    
    fclose(file);
    return count;
}

// Parse port range
int parse_port_range(const char* range_str, int* ports, int max_ports) {
    char* dash = strchr(range_str, '-');
    int count = 0;
    
    if (dash) {
        // Range format: start-end
        int start = atoi(range_str);
        int end = atoi(dash + 1);
        
        if (start > 0 && end > 0 && start <= end && start <= 65535 && end <= 65535) {
            for (int port = start; port <= end && count < max_ports; port++) {
                ports[count++] = port;
            }
        }
    } else {
        // Single port
        int port = atoi(range_str);
        if (port > 0 && port <= 65535) {
            ports[count++] = port;
        }
    }
    
    return count;
}

void print_usage(const char* program_name) {
    printf("SpearBanner - Network Service Banner Grabber\n\n");
    printf("Usage: %s [OPTIONS] <target> [ports]\n\n", program_name);
    
    printf("Options:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -v, --verbose           Enable verbose output\n");
    printf("  -t, --timeout SECONDS   Connection timeout (default: %d)\n", BANNER_TIMEOUT);
    printf("  -T, --threads NUM       Number of threads (default: %d)\n", DEFAULT_THREADS);
    printf("  -o, --output FILE       Export results to text file\n");
    printf("  -c, --csv FILE          Export results to CSV file\n");
    printf("  -j, --json FILE         Export results to JSON file\n");
    printf("  -d, --database FILE     Save results to SQLite database\n");
    printf("  -f, --file FILE         Load targets from file (host:port format)\n");
    printf("  -p, --ports RANGE       Port range (e.g., 80-90 or 80,443,8080)\n");
    
    printf("\nExamples:\n");
    printf("  %s google.com 80              # Single port\n", program_name);
    printf("  %s google.com 80-90           # Port range\n", program_name);
    printf("  %s google.com -p 80,443,8080  # Multiple ports\n", program_name);
    printf("  %s -f targets.txt             # From file\n", program_name);
    printf("  %s -v -t 5 -T 20 -o results.txt google.com 80-1000\n", program_name);
    
    printf("\nFile format (one per line):\n");
    printf("  google.com:80\n");
    printf("  192.168.1.1:22\n");
    printf("  example.com     # Will scan common ports\n");
}

int main(int argc, char* argv[]) {
    // Command line options
    int opt;
    int threads = DEFAULT_THREADS;
    char* targets_file = NULL;
    char* output_file = NULL;
    char* csv_file = NULL;
    char* json_file = NULL;
    char* db_file = NULL;
    char* port_range = NULL;
    
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"timeout", required_argument, 0, 't'},
        {"threads", required_argument, 0, 'T'},
        {"output", required_argument, 0, 'o'},
        {"csv", required_argument, 0, 'c'},
        {"json", required_argument, 0, 'j'},
        {"database", required_argument, 0, 'd'},
        {"file", required_argument, 0, 'f'},
        {"ports", required_argument, 0, 'p'},
        {0, 0, 0, 0}
    };
    
    // Create configuration
    banner_config_t* config = create_banner_config();
    if (!config) {
        fprintf(stderr, "Error: Failed to create configuration\n");
        return 1;
    }
    
    // Parse command line arguments
    while ((opt = getopt_long(argc, argv, "hvt:T:o:c:j:d:f:p:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                free_banner_config(config);
                return 0;
            case 'v':
                config->verbose = 1;
                break;
            case 't':
                config->timeout = atoi(optarg);
                if (config->timeout <= 0) config->timeout = BANNER_TIMEOUT;
                break;
            case 'T':
                threads = atoi(optarg);
                if (threads <= 0) threads = DEFAULT_THREADS;
                if (threads > MAX_THREADS) threads = MAX_THREADS;
                break;
            case 'o':
                output_file = strdup(optarg);
                break;
            case 'c':
                csv_file = strdup(optarg);
                break;
            case 'j':
                json_file = strdup(optarg);
                break;
            case 'd':
                db_file = strdup(optarg);
                break;
            case 'f':
                targets_file = strdup(optarg);
                break;
            case 'p':
                port_range = strdup(optarg);
                break;
            default:
                print_usage(argv[0]);
                free_banner_config(config);
                return 1;
        }
    }
    
    // Initialize targets array
    target_t* targets = malloc(sizeof(target_t) * MAX_TARGETS);
    if (!targets) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free_banner_config(config);
        return 1;
    }
    
    int target_count = 0;
    
    // Load targets
    if (targets_file) {
        // From file
        target_count = load_targets_from_file(targets_file, targets, MAX_TARGETS);
        if (target_count < 0) {
            free(targets);
            free_banner_config(config);
            return 1;
        }
    } else if (optind < argc) {
        // From command line
        const char* host = argv[optind];
        
        if (port_range) {
            // Parse port range from -p option
            int ports[1000];
            int port_count = 0;
            
            // Handle comma-separated ports
            char* ports_copy = strdup(port_range);
            char* token = strtok(ports_copy, ",");
            
            while (token && port_count < 1000) {
                int range_ports[100];
                int range_count = parse_port_range(token, range_ports, 100);
                
                for (int i = 0; i < range_count && port_count < 1000; i++) {
                    ports[port_count++] = range_ports[i];
                }
                
                token = strtok(NULL, ",");
            }
            free(ports_copy);
            
            // Create targets
            for (int i = 0; i < port_count && target_count < MAX_TARGETS; i++) {
                strncpy(targets[target_count].host, host, MAX_HOST_SIZE - 1);
                targets[target_count].port = ports[i];
                target_count++;
            }
            
        } else if (optind + 1 < argc) {
            // Port range from second argument
            const char* ports_arg = argv[optind + 1];
            int ports[1000];
            int port_count = parse_port_range(ports_arg, ports, 1000);
            
            for (int i = 0; i < port_count && target_count < MAX_TARGETS; i++) {
                strncpy(targets[target_count].host, host, MAX_HOST_SIZE - 1);
                targets[target_count].port = ports[i];
                target_count++;
            }
        } else {
            // Default to common ports
            int common_ports[] = {21, 22, 23, 25, 53, 80, 110, 143, 443, 993, 995, 8080, -1};
            for (int i = 0; common_ports[i] != -1 && target_count < MAX_TARGETS; i++) {
                strncpy(targets[target_count].host, host, MAX_HOST_SIZE - 1);
                targets[target_count].port = common_ports[i];
                target_count++;
            }
        }
    } else {
        fprintf(stderr, "Error: No targets specified\n");
        print_usage(argv[0]);
        free(targets);
        free_banner_config(config);
        return 1;
    }
    
    if (target_count == 0) {
        fprintf(stderr, "Error: No valid targets found\n");
        free(targets);
        free_banner_config(config);
        return 1;
    }
    
    printf("SpearBanner starting with %d targets, %d threads, %ds timeout\n",
           target_count, threads, config->timeout);
    
    // Create collection
    banner_collection_t* collection = create_banner_collection(target_count);
    if (!collection) {
        fprintf(stderr, "Error: Failed to create result collection\n");
        free(targets);
        free_banner_config(config);
        return 1;
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize thread data
    thread_data.targets = targets;
    thread_data.target_count = target_count;
    thread_data.current_index = 0;
    thread_data.collection = collection;
    thread_data.config = config;
    pthread_mutex_init(&thread_data.mutex, NULL);
    
    // Create and start threads
    pthread_t* thread_ids = malloc(sizeof(pthread_t) * threads);
    int* thread_nums = malloc(sizeof(int) * threads);
    
    for (int i = 0; i < threads; i++) {
        thread_nums[i] = i + 1;
        if (pthread_create(&thread_ids[i], NULL, banner_worker, &thread_nums[i]) != 0) {
            fprintf(stderr, "Error: Failed to create thread %d\n", i + 1);
        }
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < threads; i++) {
        pthread_join(thread_ids[i], NULL);
    }
    
    // Print summary
    printf("\n");
    print_banner_summary(collection);
    print_service_statistics(collection);
    
    // Export results
    if (output_file) {
        printf("\nExporting results to text file: %s\n", output_file);
        if (export_results_to_text(collection, output_file) == 0) {
            printf("Text export completed successfully\n");
        } else {
            printf("Text export failed\n");
        }
    }
    
    if (csv_file) {
        printf("Exporting results to CSV file: %s\n", csv_file);
        if (export_results_to_csv(collection, csv_file) == 0) {
            printf("CSV export completed successfully\n");
        } else {
            printf("CSV export failed\n");
        }
    }
    
    if (json_file) {
        printf("Exporting results to JSON file: %s\n", json_file);
        if (export_results_to_json(collection, json_file) == 0) {
            printf("JSON export completed successfully\n");
        } else {
            printf("JSON export failed\n");
        }
    }
    
    if (db_file) {
        printf("Saving results to database: %s\n", db_file);
        if (save_collection_to_database(db_file, collection) == 0) {
            printf("Database save completed successfully\n");
        } else {
            printf("Database save failed\n");
        }
    }
    
    // Cleanup
    pthread_mutex_destroy(&thread_data.mutex);
    free(thread_ids);
    free(thread_nums);
    free(targets);
    free_banner_collection(collection);
    free_banner_config(config);
    
    if (output_file) free(output_file);
    if (csv_file) free(csv_file);
    if (json_file) free(json_file);
    if (db_file) free(db_file);
    if (targets_file) free(targets_file);
    if (port_range) free(port_range);
    
    return 0;
}