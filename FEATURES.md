# Spear Scanner Features

A comprehensive network port scanner with advanced scanning techniques, service detection, and banner grabbing capabilities.

## 🎯 Core Scanning Features

### Scan Types
- **TCP Connect Scan (-sT)** - Standard TCP connection scan (default)
- **SYN Scan (-sS)** - Stealth SYN scan (requires root)
- **ACK Scan (-sA)** - Firewall detection scan (requires root)
- **XMAS Scan (-sX)** - XMAS tree scan (requires root)

### Hybrid Scanning Technology
- **Intelligent Fallback** - Raw socket scans automatically verify results with connect scans
- **Firewall Bypass** - Multiple scan types to detect filtered vs closed ports
- **Accuracy Enhancement** - Combines stealth scanning with reliable detection

## 🔍 Port Specification Options

### Single Ports
```bash
./spear target.com 80
```

### Port Ranges
```bash
./spear target.com 80-443
./spear target.com 1-1000
```

### Predefined Port Lists
- **quick** - Essential services (21 ports)
- **top10** - Most common ports (10 ports)
- **top50** - Good coverage (50 ports)
- **top100** - Balanced scan (100 ports)
- **top1000** - Comprehensive scan (1000 ports)

## 🏴‍☠️ Banner Grabbing System

### Service Detection
- **Automatic Protocol Detection** - Identifies services by port
- **Service-Specific Probes** - Optimized for HTTP, SSH, FTP, SMTP, etc.
- **Real-time Banner Collection** - Grabs banners during port scanning

### Supported Services
- HTTP/HTTPS (ports 80, 8080, 443, 8443)
- SSH (port 22)
- FTP (port 21)
- SMTP (port 25)
- Telnet (port 23)
- POP3/IMAP (ports 110, 143)
- And more...

## 📊 Export & Storage Options

### Text Export
```bash
./spear -b -o results.txt target.com quick
```

### Database Storage
```bash
./spear -b -d scan_results.db target.com top100
```

### Multiple Formats
- **Text Files** - Human-readable format
- **CSV Files** - Spreadsheet compatible
- **JSON Files** - Structured data
- **SQLite Database** - Queryable storage

## 🔧 Advanced Options

### Threading Control
```bash
./spear target.com 80-1000 50  # 50 threads
```

### Scan Timing
```bash
./spear --scan-delay 100 target.com quick  # 100ms delay
```

### Banner Timeout
```bash
./spear -b -bt 15 target.com 80-90  # 15 second timeout
```

### Verbose Output
```bash
./spear -v -b target.com quick  # Detailed output
```

## 🗄️ Database Querying

### View Statistics
```bash
./spear-banner-viewer -s results.db
```

### Filter Results
```bash
# Filter by host
./spear-banner-viewer -H target.com results.db

# Filter by service
./spear-banner-viewer -S http results.db

# Filter by port
./spear-banner-viewer -p 80 results.db
```

### Search Banner Content
```bash
# Search for specific software
./spear-banner-viewer --search "Apache" results.db

# Search for version information
./spear-banner-viewer --search "OpenSSH" -f detailed results.db
```

### Export From Database
```bash
# Export to CSV
./spear-banner-viewer --success -f csv results.db > report.csv

# Show detailed view
./spear-banner-viewer -f detailed results.db
```

## 🚀 Usage Examples

### Basic Scans
```bash
# Simple port scan
./spear google.com 80

# Range scan with banners
./spear -b google.com 80-443

# Quick service discovery
./spear -b scanme.nmap.org quick
```

### Stealth Scans
```bash
# SYN scan (requires root)
sudo ./spear -sS target.com 1-1000

# SYN scan with banners and database export
sudo ./spear -sS -b -d stealth.db target.com top100

# Rate-limited stealth scan
sudo ./spear -sS --scan-delay 200 -b target.com top1000
```

### Comprehensive Assessment
```bash
# Full scan with all outputs
./spear -sT -b -v -o scan.txt -d scan.db target.com top1000 100

# Multiple targets from file
./spear-banner -f targets.txt -b -d comprehensive.db

# Post-scan analysis
./spear-banner-viewer -s comprehensive.db
./spear-banner-viewer --search "Server:" -f detailed comprehensive.db
```

### Firewall Testing
```bash
# ACK scan for firewall detection
sudo ./spear -sA target.com 1-100

# XMAS scan for firewall bypass
sudo ./spear -sX target.com quick
```

## 🛡️ Security Features

### Root Privilege Handling
- **Automatic Detection** - Checks for required privileges
- **Graceful Fallback** - Falls back to connect scan if needed
- **Clear Warnings** - Informs user of privilege requirements

### Scan Rate Control
- **Configurable Delays** - Prevent overwhelming targets
- **Thread Staggering** - Reduces initial burst load
- **Timeout Management** - Prevents hanging connections

### Stealth Capabilities
- **Raw Socket Support** - Low-level packet crafting
- **Hybrid Verification** - Maintains stealth while ensuring accuracy
- **Firewall Detection** - Identifies filtering vs blocking

## 🎛️ Command Reference

### Scan Options
| Flag | Long Form | Description |
|------|-----------|-------------|
| `-sT` | `--tcp-connect` | TCP Connect scan (default) |
| `-sS` | `--syn` | SYN scan (requires root) |
| `-sA` | `--ack` | ACK scan (requires root) |
| `-sX` | `--xmas` | XMAS scan (requires root) |
| | `--scan-delay MS` | Delay between scans |

### Banner Options
| Flag | Long Form | Description |
|------|-----------|-------------|
| `-b` | `--banner` | Enable banner grabbing |
| `-bt SEC` | `--banner-timeout SEC` | Banner timeout |

### Output Options
| Flag | Long Form | Description |
|------|-----------|-------------|
| `-o FILE` | `--output FILE` | Export to text file |
| `-d FILE` | `--database FILE` | Save to SQLite database |
| `-v` | `--verbose` | Verbose output |

### Database Viewer Options
| Flag | Long Form | Description |
|------|-----------|-------------|
| `-s` | `--stats` | Show database statistics |
| `-H HOST` | `--host HOST` | Filter by host |
| `-p PORT` | `--port PORT` | Filter by port |
| `-S SERVICE` | `--service SERVICE` | Filter by service |
| | `--search TERM` | Search banner content |
| | `--success` | Show only successful results |

## 📈 Performance Features

### Multi-Threading
- **Configurable Threads** - Scale from 1 to 100 threads
- **Optimal Defaults** - Automatic thread selection
- **Load Balancing** - Efficient work distribution

### Scan Methods
- **Individual Threads** - One thread per port (small ranges)
- **Thread Pool** - Shared queue approach (medium ranges)
- **Lock-Free** - Atomic operations (large ranges)
- **Array-Based** - Optimized for predefined port lists

### Memory Management
- **Efficient Storage** - Minimal memory footprint
- **Result Collections** - Dynamic array expansion
- **Resource Cleanup** - Automatic memory management

## 🔍 Integration Architecture

### Modular Design
- **Scanner Modules** - Pluggable scan types
- **Banner System** - Separate but integrated
- **Export Engine** - Multiple output formats
- **Database Layer** - SQLite integration

### Thread Safety
- **Mutex Protection** - Safe shared data access
- **Atomic Operations** - Lock-free where possible
- **Result Synchronization** - Thread-safe collections

### Error Handling
- **Graceful Degradation** - Continue on individual failures
- **Resource Management** - Proper cleanup on errors
- **User Feedback** - Clear error messages

## 🛠️ Build System

### Compilation Targets
```bash
make all      # Build scanner and tools
make tools    # Build banner tools only
make debug    # Debug version
make clean    # Clean build artifacts
```

### Dependencies
- **GCC** - C11 compiler
- **SQLite3** - Database support
- **pthread** - Multi-threading
- **Raw socket support** - For advanced scans

### Installation
```bash
make install  # Install to /usr/local/bin
```

This comprehensive feature set makes Spear one of the most versatile network scanners available, combining stealth scanning capabilities with practical service discovery and analysis tools.