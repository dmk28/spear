#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sqlite3.h>
#include <time.h>

#define MAX_QUERY_SIZE 1024
#define MAX_OUTPUT_SIZE 4096

typedef struct {
    int show_failed;
    int show_successful;
    char *host_filter;
    char *service_filter;
    int port_filter;
    char *output_format;
    int limit;
    int verbose;
} query_options_t;

// Callback function for SQLite query results
static int result_callback(void *data, int argc, char **argv, char **col_names) {
    query_options_t *opts = (query_options_t*)data;
    
    if (strcmp(opts->output_format, "table") == 0) {
        // Table format (default)
        printf("%-20s %-6s %-12s %-10s %-19s %s\n", 
               argv[1] ? argv[1] : "", // host
               argv[2] ? argv[2] : "", // port
               argv[3] ? argv[3] : "", // service
               argv[6] && atoi(argv[6]) ? "SUCCESS" : "FAILED", // success
               argv[5] ? ctime(&(time_t){atol(argv[5])}) : "", // timestamp (removes newline in output)
               argv[4] ? (strlen(argv[4]) > 50 ? "..." : argv[4]) : ""); // banner (truncated)
    } else if (strcmp(opts->output_format, "csv") == 0) {
        // CSV format
        printf("\"%s\",%s,\"%s\",\"%s\",%s,\"%s\"\n",
               argv[1] ? argv[1] : "",
               argv[2] ? argv[2] : "",
               argv[3] ? argv[3] : "",
               argv[6] && atoi(argv[6]) ? "success" : "failed",
               argv[5] ? argv[5] : "",
               argv[4] ? argv[4] : "");
    } else if (strcmp(opts->output_format, "detailed") == 0) {
        // Detailed format
        printf("ID: %s\n", argv[0] ? argv[0] : "");
        printf("Host: %s\n", argv[1] ? argv[1] : "");
        printf("Port: %s\n", argv[2] ? argv[2] : "");
        printf("Service: %s\n", argv[3] ? argv[3] : "");
        printf("Status: %s\n", argv[6] && atoi(argv[6]) ? "SUCCESS" : "FAILED");
        if (argv[5]) {
            time_t timestamp = atol(argv[5]);
            printf("Timestamp: %s", ctime(&timestamp));
        }
        printf("Banner:\n%s\n", argv[4] ? argv[4] : "");
        printf("----------------------------------------\n");
    }
    
    return 0;
}

// Print table header
void print_table_header() {
    printf("%-20s %-6s %-12s %-10s %-19s %s\n", 
           "HOST", "PORT", "SERVICE", "STATUS", "TIMESTAMP", "BANNER");
    printf("%-20s %-6s %-12s %-10s %-19s %s\n", 
           "--------------------", "------", "------------", "----------", 
           "-------------------", "------");
}

// Print CSV header
void print_csv_header() {
    printf("host,port,service,status,timestamp,banner\n");
}

// Build SQL query based on options
void build_query(query_options_t *opts, char *query, int query_size) {
    strcpy(query, "SELECT id, host, port, service, banner, timestamp, success FROM banners");
    
    int has_where = 0;
    
    // Add WHERE clauses
    if (opts->host_filter) {
        strcat(query, has_where ? " AND" : " WHERE");
        strcat(query, " host LIKE '%");
        strcat(query, opts->host_filter);
        strcat(query, "%'");
        has_where = 1;
    }
    
    if (opts->service_filter) {
        strcat(query, has_where ? " AND" : " WHERE");
        strcat(query, " service = '");
        strcat(query, opts->service_filter);
        strcat(query, "'");
        has_where = 1;
    }
    
    if (opts->port_filter > 0) {
        char port_str[16];
        sprintf(port_str, "%d", opts->port_filter);
        strcat(query, has_where ? " AND" : " WHERE");
        strcat(query, " port = ");
        strcat(query, port_str);
        has_where = 1;
    }
    
    if (opts->show_successful && !opts->show_failed) {
        strcat(query, has_where ? " AND" : " WHERE");
        strcat(query, " success = 1");
        has_where = 1;
    } else if (opts->show_failed && !opts->show_successful) {
        strcat(query, has_where ? " AND" : " WHERE");
        strcat(query, " success = 0");
        has_where = 1;
    }
    
    // Add ORDER BY
    strcat(query, " ORDER BY timestamp DESC");
    
    // Add LIMIT
    if (opts->limit > 0) {
        char limit_str[32];
        sprintf(limit_str, " LIMIT %d", opts->limit);
        strcat(query, limit_str);
    }
}

// Show database statistics
int show_statistics(const char *db_file) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc;
    
    rc = sqlite3_open_v2(db_file, &db, SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error: Cannot open database '%s': %s\n", db_file, sqlite3_errmsg(db));
        return -1;
    }
    
    printf("Database Statistics for: %s\n", db_file);
    printf("================================\n");
    
    // Total records
    const char *total_sql = "SELECT COUNT(*) FROM banners";
    rc = sqlite3_prepare_v2(db, total_sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("Total records: %d\n", sqlite3_column_int(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    
    // Successful vs failed
    const char *success_sql = "SELECT success, COUNT(*) FROM banners GROUP BY success";
    rc = sqlite3_prepare_v2(db, success_sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int success = sqlite3_column_int(stmt, 0);
            int count = sqlite3_column_int(stmt, 1);
            printf("%s: %d\n", success ? "Successful" : "Failed", count);
        }
        sqlite3_finalize(stmt);
    }
    
    // Top hosts
    printf("\nTop 10 hosts:\n");
    const char *hosts_sql = "SELECT host, COUNT(*) as count FROM banners GROUP BY host ORDER BY count DESC LIMIT 10";
    rc = sqlite3_prepare_v2(db, hosts_sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("  %s: %d\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
        }
        sqlite3_finalize(stmt);
    }
    
    // Top services
    printf("\nTop services:\n");
    const char *services_sql = "SELECT service, COUNT(*) as count FROM banners GROUP BY service ORDER BY count DESC LIMIT 10";
    rc = sqlite3_prepare_v2(db, services_sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("  %s: %d\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
        }
        sqlite3_finalize(stmt);
    }
    
    // Date range
    const char *date_sql = "SELECT MIN(timestamp), MAX(timestamp) FROM banners";
    rc = sqlite3_prepare_v2(db, date_sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            time_t min_time = sqlite3_column_int64(stmt, 0);
            time_t max_time = sqlite3_column_int64(stmt, 1);
            printf("\nDate range:\n");
            printf("  From: %s", ctime(&min_time));
            printf("  To:   %s", ctime(&max_time));
        }
        sqlite3_finalize(stmt);
    }
    
    sqlite3_close(db);
    return 0;
}

// Search banners by content
int search_banners(const char *db_file, const char *search_term, query_options_t *opts) {
    sqlite3 *db;
    char *err_msg = 0;
    int rc;
    
    rc = sqlite3_open_v2(db_file, &db, SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error: Cannot open database '%s': %s\n", db_file, sqlite3_errmsg(db));
        return -1;
    }
    
    char query[MAX_QUERY_SIZE];
    snprintf(query, sizeof(query), 
             "SELECT id, host, port, service, banner, timestamp, success "
             "FROM banners WHERE banner LIKE '%%%s%%' "
             "ORDER BY timestamp DESC%s",
             search_term,
             opts->limit > 0 ? " LIMIT ?" : "");
    
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error preparing query: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    
    if (opts->limit > 0) {
        sqlite3_bind_int(stmt, 1, opts->limit);
    }
    
    printf("Searching for banners containing: '%s'\n", search_term);
    printf("=====================================\n");
    
    if (strcmp(opts->output_format, "table") == 0) {
        print_table_header();
    } else if (strcmp(opts->output_format, "csv") == 0) {
        print_csv_header();
    }
    
    int results = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results++;
        
        // Simulate the callback data structure
        char *argv[7];
        for (int i = 0; i < 7; i++) {
            const char *text = (const char*)sqlite3_column_text(stmt, i);
            argv[i] = text ? (char*)text : NULL;
        }
        
        result_callback(opts, 7, argv, NULL);
    }
    
    printf("\nFound %d matching results\n", results);
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}

void print_usage(const char *program_name) {
    printf("SpearBanner Database Viewer\n\n");
    printf("Usage: %s [OPTIONS] <database_file>\n\n", program_name);
    
    printf("Options:\n");
    printf("  -h, --help                Show this help message\n");
    printf("  -s, --stats               Show database statistics\n");
    printf("  -v, --verbose             Enable verbose output\n");
    printf("  -f, --format FORMAT       Output format: table, csv, detailed (default: table)\n");
    printf("  -l, --limit NUM           Limit number of results (default: all)\n");
    printf("  -H, --host HOST           Filter by host (supports wildcards)\n");
    printf("  -p, --port PORT           Filter by port number\n");
    printf("  -S, --service SERVICE     Filter by service name\n");
    printf("  --success                 Show only successful banner grabs\n");
    printf("  --failed                  Show only failed banner grabs\n");
    printf("  --search TERM             Search banner content for term\n");
    
    printf("\nExamples:\n");
    printf("  %s banners.db                              # Show all results\n", program_name);
    printf("  %s -s banners.db                           # Show statistics\n", program_name);
    printf("  %s -H google.com banners.db                # Filter by host\n", program_name);
    printf("  %s -p 80 --success banners.db              # Show successful HTTP scans\n", program_name);
    printf("  %s --search \"Apache\" -f detailed banners.db # Search for Apache servers\n", program_name);
    printf("  %s -S ssh -l 10 banners.db                 # Show last 10 SSH banners\n", program_name);
}

int main(int argc, char *argv[]) {
    int opt;
    query_options_t opts = {0};
    int show_stats = 0;
    char *search_term = NULL;
    
    // Default options
    opts.show_failed = 1;
    opts.show_successful = 1;
    opts.output_format = "table";
    opts.limit = 0;
    
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"stats", no_argument, 0, 's'},
        {"verbose", no_argument, 0, 'v'},
        {"format", required_argument, 0, 'f'},
        {"limit", required_argument, 0, 'l'},
        {"host", required_argument, 0, 'H'},
        {"port", required_argument, 0, 'p'},
        {"service", required_argument, 0, 'S'},
        {"success", no_argument, 0, 1001},
        {"failed", no_argument, 0, 1002},
        {"search", required_argument, 0, 1003},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "hsvf:l:H:p:S:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 's':
                show_stats = 1;
                break;
            case 'v':
                opts.verbose = 1;
                break;
            case 'f':
                if (strcmp(optarg, "table") == 0 || strcmp(optarg, "csv") == 0 || strcmp(optarg, "detailed") == 0) {
                    opts.output_format = strdup(optarg);
                } else {
                    fprintf(stderr, "Error: Invalid format '%s'. Use: table, csv, or detailed\n", optarg);
                    return 1;
                }
                break;
            case 'l':
                opts.limit = atoi(optarg);
                if (opts.limit <= 0) {
                    fprintf(stderr, "Error: Limit must be a positive number\n");
                    return 1;
                }
                break;
            case 'H':
                opts.host_filter = strdup(optarg);
                break;
            case 'p':
                opts.port_filter = atoi(optarg);
                if (opts.port_filter <= 0 || opts.port_filter > 65535) {
                    fprintf(stderr, "Error: Port must be between 1 and 65535\n");
                    return 1;
                }
                break;
            case 'S':
                opts.service_filter = strdup(optarg);
                break;
            case 1001: // --success
                opts.show_successful = 1;
                opts.show_failed = 0;
                break;
            case 1002: // --failed
                opts.show_failed = 1;
                opts.show_successful = 0;
                break;
            case 1003: // --search
                search_term = strdup(optarg);
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Check for database file argument
    if (optind >= argc) {
        fprintf(stderr, "Error: Database file required\n");
        print_usage(argv[0]);
        return 1;
    }
    
    const char *db_file = argv[optind];
    
    // Check if database file exists
    if (access(db_file, R_OK) != 0) {
        fprintf(stderr, "Error: Cannot access database file '%s'\n", db_file);
        return 1;
    }
    
    // Show statistics if requested
    if (show_stats) {
        return show_statistics(db_file);
    }
    
    // Search banners if search term provided
    if (search_term) {
        return search_banners(db_file, search_term, &opts);
    }
    
    // Regular query
    sqlite3 *db;
    char *err_msg = 0;
    int rc;
    
    rc = sqlite3_open_v2(db_file, &db, SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error: Cannot open database '%s': %s\n", db_file, sqlite3_errmsg(db));
        return 1;
    }
    
    char query[MAX_QUERY_SIZE];
    build_query(&opts, query, sizeof(query));
    
    if (opts.verbose) {
        printf("Executing query: %s\n\n", query);
    }
    
    // Print header
    if (strcmp(opts.output_format, "table") == 0) {
        print_table_header();
    } else if (strcmp(opts.output_format, "csv") == 0) {
        print_csv_header();
    }
    
    // Execute query
    rc = sqlite3_exec(db, query, result_callback, &opts, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error executing query: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }
    
    sqlite3_close(db);
    
    // Cleanup
    if (opts.host_filter) free(opts.host_filter);
    if (opts.service_filter) free(opts.service_filter);
    if (search_term) free(search_term);
    if (strcmp(opts.output_format, "table") != 0) free(opts.output_format);
    
    return 0;
}