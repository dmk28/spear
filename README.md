# Spear Network Scanner

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C11](https://img.shields.io/badge/standard-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)](https://www.kernel.org/)

A high-performance, multi-threaded network port scanner with integrated banner grabbing capabilities. Spear combines stealth scanning techniques with practical service discovery and analysis tools.

## Features

### 🎯 Advanced Scanning Techniques
- **TCP Connect Scan** (`-sT`) - Standard connection-based scanning
- **SYN Scan** (`-sS`) - Stealth half-open scanning (requires root)
- **ACK Scan** (`-sA`) - Firewall detection and rule mapping (requires root)
- **XMAS Scan** (`-sX`) - Advanced evasion technique (requires root)

### 🔍 Intelligent Service Detection
- **Real-time Banner Grabbing** - Captures service banners during scanning
- **Protocol-Specific Probes** - Optimized for HTTP, SSH, FTP, SMTP, and more
- **Hybrid Verification** - Combines raw socket scans with connection verification
- **Service Fingerprinting** - Automatic service identification by port and banner

### 📊 Flexible Output & Storage
- **Multiple Export Formats** - Text, CSV, JSON, and SQLite database
- **Queryable Database** - Built-in SQLite support with analysis tools
- **Professional Reporting** - Clean, parseable output for automation
- **Banner Search** - Content-based filtering and analysis

### ⚡ High Performance
- **Multi-threaded Architecture** - Configurable thread pools (1-100 threads)
- **Lock-free Algorithms** - Optimized for large-scale scanning
- **Adaptive Scanning** - Automatic method selection based on target size
- **Memory Efficient** - Minimal footprint with dynamic allocation

## Quick Start

### Installation

```bash
# Clone and build
git clone <repository-url>
cd spear
make

# Install system-wide (optional)
sudo make install
```

### Basic Usage

```bash
# Simple port scan
./spear google.com 80

# Port range with banners
./spear -b google.com 80-443

# Stealth SYN scan (requires root)
sudo ./spear -sS target.com 1-1000

# Comprehensive scan with database export
./spear -b -d results.db target.com top1000
```

## Usage Guide

### Port Specification

| Format | Description | Example |
|--------|-------------|---------|
| Single port | `80` | `./spear target.com 80` |
| Port range | `80-443` | `./spear target.com 80-443` |
| Predefined lists | `quick`, `top10`, `top100`, `top1000` | `./spear target.com quick` |

### Scan Types

#### TCP Connect Scan (Default)
```bash
./spear target.com 80-443
./spear -sT target.com quick  # Explicit
```
Standard TCP connection scan. Reliable and doesn't require privileges.

#### SYN Scan (Stealth)
```bash
sudo ./spear -sS target.com 80-443
```
Half-open scanning technique. Stealthier but requires root privileges.

#### ACK Scan (Firewall Detection)
```bash
sudo ./spear -sA target.com 80-443
```
Used to map firewall rules and detect packet filtering.

#### XMAS Scan (Evasion)
```bash
sudo ./spear -sX target.com 80-443
```
Advanced evasion technique using TCP flag manipulation.

### Banner Grabbing

Enable banner grabbing to identify services and versions:

```bash
# Basic banner grabbing
./spear -b target.com 80-443

# With verbose output
./spear -b -v target.com quick

# Custom timeout
./spear -b -bt 15 target.com top100

# Export banners to file
./spear -b -o banners.txt target.com quick

# Save to database
./spear -b -d scan.db target.com top1000
```

### Database Analysis

Use the built-in database viewer to analyze results:

```bash
# View statistics
./spear-banner-viewer -s results.db

# Filter by host
./spear-banner-viewer -H target.com results.db

# Search banner content
./spear-banner-viewer --search "Apache" results.db

# Export to CSV
./spear-banner-viewer --success -f csv results.db > report.csv
```

## Command Reference

### Main Scanner Options

| Short | Long | Description |
|-------|------|-------------|
| `-sT` | `--tcp-connect` | TCP Connect scan (default) |
| `-sS` | `--syn` | SYN scan (requires root) |
| `-sA` | `--ack` | ACK scan (requires root) |
| `-sX` | `--xmas` | XMAS scan (requires root) |
| `-b` | `--banner` | Enable banner grabbing |
| `-bt SEC` | `--banner-timeout SEC` | Banner grab timeout (default: 10s) |
| `-o FILE` | `--output FILE` | Export results to text file |
| `-d FILE` | `--database FILE` | Save results to SQLite database |
| `-v` | `--verbose` | Enable verbose output |
| `--scan-delay MS` | | Delay between scans in milliseconds |

### Database Viewer Options

| Short | Long | Description |
|-------|------|-------------|
| `-s` | `--stats` | Show database statistics |
| `-H HOST` | `--host HOST` | Filter by host |
| `-p PORT` | `--port PORT` | Filter by port |
| `-S SERVICE` | `--service SERVICE` | Filter by service |
| `-f FORMAT` | `--format FORMAT` | Output format (table, csv, detailed) |
| `--search TERM` | | Search banner content |
| `--success` | | Show only successful scans |
| `--failed` | | Show only failed scans |

## Examples

### Quick Network Assessment
```bash
# Scan common ports with banners
./spear -b target.com quick

# Comprehensive scan with database storage
./spear -b -d assessment.db target.com top1000 50

# Analyze results
./spear-banner-viewer -s assessment.db
```

### Stealth Reconnaissance
```bash
# SYN scan with rate limiting
sudo ./spear -sS --scan-delay 100 target.com top100

# Multi-target stealth scan
sudo ./spear-banner -f targets.txt -sS -d stealth.db

# Search for specific services
./spear-banner-viewer --search "OpenSSH" -f detailed stealth.db
```

### Service Discovery
```bash
# Web service enumeration
./spear -b target.com 80,8080,8000,8443,443

# Mail server analysis
./spear -b target.com 25,110,143,993,995

# Database service discovery
./spear -b target.com 1433,3306,5432,27017,6379
```

### Large-Scale Scanning
```bash
# High-throughput scan
./spear -b -d large_scan.db target.com top1000 100

# Export for reporting
./spear-banner-viewer --success -f csv large_scan.db > services.csv

# Generate summary statistics
./spear-banner-viewer -s large_scan.db
```

## Architecture

### Multi-threading Models

Spear automatically selects the optimal threading approach based on scan size:

- **Individual Threads** - One thread per port (small ranges ≤100 ports)
- **Lock-free Queue** - Atomic operations for high performance (large ranges)
- **Thread Pool** - Shared work queue for medium ranges

### Banner Grabbing System

The integrated banner grabbing system:

1. **Service Detection** - Identifies services by port and protocol
2. **Protocol Probes** - Sends appropriate requests (HTTP GET, SSH handshake, etc.)
3. **Response Processing** - Captures and cleans banner data
4. **Real-time Collection** - Grabs banners immediately when ports are found open

### Database Schema

SQLite database structure for persistent storage:

```sql
CREATE TABLE banners (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    host TEXT NOT NULL,
    port INTEGER NOT NULL,
    service TEXT,
    banner TEXT,
    timestamp INTEGER,
    success INTEGER
);
```

## Performance

### Benchmarks

Typical performance on modern hardware:

- **Small ranges (1-100 ports)**: ~50-200 ports/second
- **Large ranges (1000+ ports)**: ~500-1000 ports/second
- **Banner grabbing**: Adds ~100-500ms per open port
- **Memory usage**: ~1-5MB base + ~4KB per banner result

### Optimization Tips

1. **Increase thread count** for large ranges: `-T 50`
2. **Reduce banner timeout** for faster scans: `-bt 5`
3. **Use appropriate scan type** for your needs
4. **Add scan delays** to avoid overwhelming targets: `--scan-delay 100`

## Security Considerations

### Privileges
- **Raw socket scans** (`-sS`, `-sA`, `-sX`) require root privileges
- **Connect scans** (`-sT`) work with normal user privileges
- **Firewall detection** may require raw socket access

### Responsible Usage
- **Rate limiting** - Use `--scan-delay` to avoid overwhelming targets
- **Legal compliance** - Only scan systems you own or have permission to test
- **Network etiquette** - Be considerate of network resources and monitoring

### Stealth Features
- **SYN scanning** - Leaves minimal traces in target logs
- **Custom timing** - Configurable delays for evasion
- **Hybrid verification** - Balances stealth with accuracy

## Dependencies

### Build Requirements
- **GCC** with C11 support
- **Make** build system
- **SQLite3** development libraries
- **pthread** library (usually included)

### Runtime Requirements
- **Linux** operating system
- **Raw socket support** for advanced scan types
- **SQLite3** for database functionality

### Installation Commands

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install gcc make libsqlite3-dev
```

**RHEL/CentOS/Fedora:**
```bash
sudo yum install gcc make sqlite-devel
# or
sudo dnf install gcc make sqlite-devel
```

## Build Options

```bash
# Standard build
make

# Debug build with symbols
make debug

# Performance optimized build
make perf

# Build with all scanner modules
make full

# Clean build artifacts
make clean

# Install system-wide
sudo make install

# Uninstall
sudo make uninstall
```

## Troubleshooting

### Common Issues

**Permission Denied (Raw Scans)**
```bash
# Run with sudo for SYN/ACK/XMAS scans
sudo ./spear -sS target.com 80-443
```

**Database Locked**
```bash
# Ensure no other processes are using the database
lsof results.db
```

**High Memory Usage**
```bash
# Reduce thread count for large scans
./spear target.com top1000 20
```

**Slow Performance**
```bash
# Increase timeout for slow networks
./spear -bt 15 target.com top100
```

### Debug Mode

Enable verbose output for troubleshooting:
```bash
./spear -v -b target.com quick
```

## Contributing

Spear is designed with modularity in mind:

- **Scanner modules** in `scanners/` directory
- **Utilities** in `utils/` directory  
- **Tools** in `tools/` directory
- **Main scanner** in `src/main.c`

### Development Build
```bash
make debug
gdb ./spear_debug
```

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- **Network security community** for scanning techniques and methodologies
- **SQLite team** for the excellent embedded database
- **GNU/Linux** community for robust networking stack and tools

---

**Spear** - Professional network reconnaissance and service discovery.