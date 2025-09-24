#ifndef DEFAULT_PORTS_H
#define DEFAULT_PORTS_H

// Array sizes
#define TOP_10_PORTS_SIZE 10
#define TOP_50_PORTS_SIZE 50
#define TOP_100_PORTS_SIZE 100
#define TOP_1000_PORTS_SIZE 1000
#define QUICK_SCAN_PORTS_SIZE 21

// Port array declarations
extern const int TOP_10_PORTS[TOP_10_PORTS_SIZE];
extern const int TOP_50_PORTS[TOP_50_PORTS_SIZE];
extern const int TOP_100_PORTS[TOP_100_PORTS_SIZE];
extern const int TOP_1000_PORTS[TOP_1000_PORTS_SIZE];
extern const int QUICK_SCAN_PORTS[QUICK_SCAN_PORTS_SIZE];

// Enumeration for port list types
typedef enum {
    PORT_LIST_QUICK = 0,
    PORT_LIST_TOP10,
    PORT_LIST_TOP50,
    PORT_LIST_TOP100,
    PORT_LIST_TOP1000
} port_list_type_t;

// Structure to hold port list information
typedef struct {
    const int *ports;
    int count;
    const char *description;
} port_list_info_t;

// Function declarations
port_list_info_t get_port_list(port_list_type_t type);
void print_available_port_lists(void);
port_list_type_t parse_port_list_name(const char *name);

#endif // DEFAULT_PORTS_H