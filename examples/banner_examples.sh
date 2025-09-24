#!/bin/bash

# SpearBanner Examples Script
# Demonstrates various ways to use the banner grabber tools

echo "SpearBanner Tool Examples"
echo "========================="
echo

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check if tools are built
if [ ! -f "../spear-banner" ] || [ ! -f "../spear-banner-viewer" ]; then
    echo -e "${RED}Error: Banner tools not found. Please build them first:${NC}"
    echo "cd .. && make tools"
    exit 1
fi

# Create example targets file
echo "Creating example targets file..."
cat > targets.txt << EOF
# Example targets file
# Format: host:port or just host (will scan common ports)
google.com:80
google.com:443
github.com:22
github.com:80
github.com:443
scanme.nmap.org:22
scanme.nmap.org:80
httpbin.org:80
httpbin.org:443
EOF

echo -e "${GREEN}Example targets file created: targets.txt${NC}"
echo

# Example 1: Basic banner grab on single host
echo -e "${BLUE}Example 1: Basic banner grab on single host${NC}"
echo "Command: ../spear-banner google.com 80"
echo -e "${YELLOW}Running...${NC}"
../spear-banner google.com 80
echo
read -p "Press Enter to continue..."
echo

# Example 2: Banner grab with port range
echo -e "${BLUE}Example 2: Banner grab with port range${NC}"
echo "Command: ../spear-banner google.com 80-85"
echo -e "${YELLOW}Running...${NC}"
../spear-banner google.com 80-85
echo
read -p "Press Enter to continue..."
echo

# Example 3: Multiple specific ports
echo -e "${BLUE}Example 3: Multiple specific ports${NC}"
echo "Command: ../spear-banner google.com -p 80,443,8080"
echo -e "${YELLOW}Running...${NC}"
../spear-banner google.com -p 80,443,8080
echo
read -p "Press Enter to continue..."
echo

# Example 4: Verbose mode with timeout and threads
echo -e "${BLUE}Example 4: Verbose mode with custom settings${NC}"
echo "Command: ../spear-banner -v -t 5 -T 5 google.com 80-90"
echo -e "${YELLOW}Running...${NC}"
../spear-banner -v -t 5 -T 5 google.com 80-90
echo
read -p "Press Enter to continue..."
echo

# Example 5: From targets file
echo -e "${BLUE}Example 5: Banner grab from targets file${NC}"
echo "Command: ../spear-banner -f targets.txt -v"
echo -e "${YELLOW}Running...${NC}"
../spear-banner -f targets.txt -v
echo
read -p "Press Enter to continue..."
echo

# Example 6: Export to different formats
echo -e "${BLUE}Example 6: Export results to different formats${NC}"
echo "Command: ../spear-banner -f targets.txt -o results.txt -c results.csv -j results.json -d results.db"
echo -e "${YELLOW}Running...${NC}"
../spear-banner -f targets.txt -o results.txt -c results.csv -j results.json -d results.db
echo
echo -e "${GREEN}Results exported to:${NC}"
echo "  - Text file: results.txt"
echo "  - CSV file: results.csv"
echo "  - JSON file: results.json"
echo "  - SQLite database: results.db"
echo
read -p "Press Enter to continue..."
echo

# Example 7: View database statistics
if [ -f "results.db" ]; then
    echo -e "${BLUE}Example 7: View database statistics${NC}"
    echo "Command: ../spear-banner-viewer -s results.db"
    echo -e "${YELLOW}Running...${NC}"
    ../spear-banner-viewer -s results.db
    echo
    read -p "Press Enter to continue..."
    echo
fi

# Example 8: Query database with filters
if [ -f "results.db" ]; then
    echo -e "${BLUE}Example 8: Query database with filters${NC}"
    echo "Command: ../spear-banner-viewer -H google.com results.db"
    echo -e "${YELLOW}Running...${NC}"
    ../spear-banner-viewer -H google.com results.db
    echo
    read -p "Press Enter to continue..."
    echo
fi

# Example 9: Search banner content
if [ -f "results.db" ]; then
    echo -e "${BLUE}Example 9: Search banner content${NC}"
    echo "Command: ../spear-banner-viewer --search \"Server:\" -f detailed results.db"
    echo -e "${YELLOW}Running...${NC}"
    ../spear-banner-viewer --search "Server:" -f detailed results.db
    echo
    read -p "Press Enter to continue..."
    echo
fi

# Example 10: Show only successful results in CSV format
if [ -f "results.db" ]; then
    echo -e "${BLUE}Example 10: Export successful results to CSV${NC}"
    echo "Command: ../spear-banner-viewer --success -f csv results.db > successful_banners.csv"
    echo -e "${YELLOW}Running...${NC}"
    ../spear-banner-viewer --success -f csv results.db > successful_banners.csv
    echo -e "${GREEN}Successful banners exported to: successful_banners.csv${NC}"
    echo
fi

# Show created files
echo -e "${GREEN}Files created during examples:${NC}"
ls -la *.txt *.csv *.json *.db 2>/dev/null || echo "No files found"
echo

# Cleanup option
echo -e "${YELLOW}Cleanup options:${NC}"
echo "To remove example files: rm -f targets.txt results.* successful_banners.csv"
echo "To keep files for further testing: leave them as-is"
echo

echo -e "${GREEN}Examples completed!${NC}"
echo
echo "Additional usage tips:"
echo "====================="
echo
echo "1. For large scans, increase thread count: -T 50"
echo "2. Adjust timeout for slow networks: -t 10"
echo "3. Use database storage for large datasets: -d filename.db"
echo "4. Search banners for specific software: --search \"Apache\""
echo "5. Filter by service type: -S http"
echo "6. View only recent results: -l 100"
echo
echo "For more options, run:"
echo "  ../spear-banner --help"
echo "  ../spear-banner-viewer --help"
