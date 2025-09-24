# Banner Grabbing Integration

SpearBanner grabbing has been fully integrated into the main Spear port scanner. You can now perform port scanning and banner grabbing in a single operation.

## Usage

### Basic Banner Grabbing

Enable banner grabbing with the `-b` or `--banner` flag:

```bash
./spear -b google.com 80
./spear --banner scanme.nmap.org 22-80
```

### Verbose Output

Use `-v` or `--verbose` to see detailed banner grabbing information:

```bash
./spear -b -v google.com 80-443
```

### Export Options

Export banner results to various formats:

```bash
# Export to text file
./spear -b -o results.txt google.com 80-90

# Save to SQLite database
./spear -b -d banners.db scanme.nmap.org quick

# Both text and database
./spear -b -o results.txt -d banners.db github.com top10
```

### Banner Timeout

Adjust the banner grabbing timeout (default: 10 seconds):

```bash
./spear -b -bt 5 slow-server.com 80-100
./spear --banner --banner-timeout 15 target.com quick
```

## Examples

### Single Port with Banner
```bash
./spear -b -v google.com 80
```
Output:
```
Resolved google.com to 142.250.78.174
Banner grabbing enabled (timeout: 10s)
Target IP: 142.250.78.174
Single port: 80
Threads: 10
Starting scan...
Thread 0: Port 80 is OPEN
Thread 0: [+] Banner grabbed for port 80: http
Scan complete. Found 1 open ports:
  Port 80

Banner Summary:
==================
Total attempts: 1
Successful: 1
Failed: 0
Success rate: 100.0%
```

### Port Range with Database Export
```bash
./spear -b -d scan_results.db scanme.nmap.org 20-25
```

### Quick Scan with All Options
```bash
./spear -b -v -o banners.txt -d comprehensive.db -bt 8 scanme.nmap.org quick
```

## Port Lists Support

Banner grabbing works with all port list options:

```bash
./spear -b -d results.db target.com quick     # 21 ports
./spear -b -d results.db target.com top10    # 10 ports  
./spear -b -d results.db target.com top100   # 100 ports
./spear -b -d results.db target.com top1000  # 1000 ports
```

## Viewing Results

### Text File Output
When using `-o filename.txt`, results are saved in human-readable format:

```
Banner Grab Results
===================
Generated: Wed Sep 24 00:45:12 2025
Total results: 1

Host: 142.250.78.174:80 (http)
Timestamp: Wed Sep 24 00:45:10 2025
Status: Success
Banner:
HTTP/1.1 301 Moved Permanently
Location: http://www.google.com/
Server: gws
...
```

### Database Queries
Use the `spear-banner-viewer` tool to query saved results:

```bash
# Show statistics
./spear-banner-viewer -s results.db

# View all results
./spear-banner-viewer results.db

# Filter by host
./spear-banner-viewer -H google.com results.db

# Search banner content
./spear-banner-viewer --search "Apache" -f detailed results.db

# Show only successful results
./spear-banner-viewer --success results.db
```

## Service Detection

The banner grabber automatically detects services and uses appropriate probes:

- **HTTP (80, 8080, 8000, 8008)**: Sends HTTP GET request
- **HTTPS (443, 8443)**: Attempts SSL connection
- **SSH (22)**: Reads SSH version banner
- **FTP (21)**: Reads FTP welcome banner
- **SMTP (25)**: Sends EHLO command
- **Telnet (23)**: Reads telnet banner
- **POP3 (110)**: Sends USER command
- **IMAP (143)**: Sends CAPABILITY command

## Performance Considerations

### Threading
- Banner grabbing respects the thread count setting
- Each open port gets its banner grabbed in the same thread that found it
- No additional threading overhead for banner operations

### Timeout Settings
- Default banner timeout: 10 seconds
- Adjust with `-bt` for slow networks or servers
- Lower timeouts speed up scans but may miss slow services

### Memory Usage
- Banner results are stored in memory during scanning
- Large scans should use database export (`-d`) for persistence
- Each banner uses up to 4KB of memory

## Integration Architecture

### Scanning Flow
1. Port scanning finds open ports
2. For each open port, banner grabbing is attempted immediately
3. Results are collected and displayed at the end
4. Export operations happen after all scanning completes

### Data Storage
- Banner results are stored in a `banner_collection_t` structure
- Thread-safe operations ensure data integrity
- SQLite database schema includes all banner metadata

### Error Handling
- Failed banner grabs are recorded but don't stop scanning
- Network timeouts are handled gracefully
- Database errors are reported but don't crash the scanner

## Command Reference

| Flag | Long Form | Description |
|------|-----------|-------------|
| `-b` | `--banner` | Enable banner grabbing |
| `-bt SEC` | `--banner-timeout SEC` | Set banner timeout (default: 10) |
| `-o FILE` | `--output FILE` | Export to text file |
| `-d FILE` | `--database FILE` | Save to SQLite database |
| `-v` | `--verbose` | Show detailed banner info |
| `-h` | `--help` | Show help message |

## Troubleshooting

### No Banners Grabbed
- Check if ports are actually open
- Increase timeout with `-bt` for slow services
- Use verbose mode (`-v`) to see what's happening

### Database Export Failures
- Ensure SQLite3 is installed on your system
- Check write permissions for the database file
- Verify disk space availability

### Incomplete Banners
- Some services send data slowly or in chunks
- Try increasing banner timeout
- Some services may not respond to generic probes

## Examples by Scenario

### Web Server Enumeration
```bash
./spear -b -v -o webservers.txt target.com 80,8080,8000,8443,443
```

### SSH Version Detection
```bash
./spear -b --search "OpenSSH" network-range 22
```

### Comprehensive Service Discovery
```bash
./spear -b -d full_scan.db -bt 15 target.com top1000 50
./spear-banner-viewer -s full_scan.db
```

### Quick Security Assessment
```bash
./spear -b -v -o security_scan.txt target-list.txt quick
```

This integration provides a seamless workflow for both port scanning and service identification in a single tool.