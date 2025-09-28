#!/bin/bash

# CI Helper Script for Spear Project
# This script helps monitor and manage GitHub Actions workflows

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# GitHub API base URL
GITHUB_API="https://api.github.com"
REPO_OWNER=$(git remote get-url origin | sed -n 's/.*github\.com[:/]\([^/]*\)\/\([^.]*\).*/\1/p')
REPO_NAME=$(git remote get-url origin | sed -n 's/.*github\.com[:/]\([^/]*\)\/\([^.]*\).*/\2/p')

# Function to print colored output
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${NC}"
}

# Function to check workflow status
check_workflows() {
    print_status $BLUE "🔍 Checking recent workflow runs..."

    if ! command -v curl &> /dev/null; then
        print_status $RED "❌ curl is required but not installed"
        exit 1
    fi

    local api_url="${GITHUB_API}/repos/${REPO_OWNER}/${REPO_NAME}/actions/runs?per_page=10"
    local response=$(curl -s "$api_url")

    if echo "$response" | grep -q "workflow_runs"; then
        print_status $GREEN "✅ Successfully fetched workflow data"

        # Parse and display workflow status
        echo "$response" | python3 -c "
import json
import sys
from datetime import datetime

data = json.load(sys.stdin)
for run in data['workflow_runs'][:5]:
    status_emoji = {
        'success': '✅',
        'failure': '❌',
        'cancelled': '🚫',
        'in_progress': '🔄',
        'queued': '⏳'
    }.get(run['status'], '❓')

    conclusion_emoji = {
        'success': '✅',
        'failure': '❌',
        'cancelled': '🚫',
        'timed_out': '⏰'
    }.get(run['conclusion'], '❓') if run['conclusion'] else ''

    created_at = datetime.fromisoformat(run['created_at'].replace('Z', '+00:00'))

    print(f\"{status_emoji} {conclusion_emoji} {run['name']} - {run['head_branch']}\")
    print(f\"   Status: {run['status']} | Started: {created_at.strftime('%Y-%m-%d %H:%M UTC')}\")
    print(f\"   URL: {run['html_url']}\")
    print()
"
    else
        print_status $RED "❌ Failed to fetch workflow data"
        echo "$response"
    fi
}

# Function to cancel running workflows
cancel_workflows() {
    print_status $YELLOW "🚫 Looking for running workflows to cancel..."

    local api_url="${GITHUB_API}/repos/${REPO_OWNER}/${REPO_NAME}/actions/runs?status=in_progress"
    local response=$(curl -s "$api_url")

    if echo "$response" | grep -q "workflow_runs"; then
        local run_ids=$(echo "$response" | python3 -c "
import json
import sys
data = json.load(sys.stdin)
for run in data['workflow_runs']:
    if run['status'] in ['in_progress', 'queued']:
        print(run['id'])
")

        if [ -n "$run_ids" ]; then
            echo "Found running workflows:"
            echo "$run_ids"
            read -p "Cancel these workflows? (y/N): " confirm

            if [[ $confirm =~ ^[Yy]$ ]]; then
                for run_id in $run_ids; do
                    print_status $YELLOW "Cancelling workflow $run_id..."
                    curl -X POST -s \
                         -H "Authorization: token $GITHUB_TOKEN" \
                         "${GITHUB_API}/repos/${REPO_OWNER}/${REPO_NAME}/actions/runs/${run_id}/cancel"
                done
                print_status $GREEN "✅ Cancellation requests sent"
            fi
        else
            print_status $GREEN "✅ No running workflows to cancel"
        fi
    fi
}

# Function to test local build
test_local_build() {
    print_status $BLUE "🔨 Testing local build..."

    if [ ! -f "Makefile" ]; then
        print_status $RED "❌ Makefile not found. Are you in the project root?"
        exit 1
    fi

    # Clean and build
    print_status $YELLOW "Cleaning previous builds..."
    make clean

    print_status $YELLOW "Building project..."
    if make all; then
        print_status $GREEN "✅ Build successful"

        # Test executables
        if [ -f "spear" ]; then
            print_status $GREEN "✅ spear executable created"
            ./spear --help 2>/dev/null || echo "Help command test completed"
        fi

        if [ -f "spear-banner" ]; then
            print_status $GREEN "✅ spear-banner executable created"
        fi

        if [ -f "spear-banner-viewer" ]; then
            print_status $GREEN "✅ spear-banner-viewer executable created"
        fi

    else
        print_status $RED "❌ Build failed"
        exit 1
    fi
}

# Function to check dependencies
check_dependencies() {
    print_status $BLUE "📋 Checking build dependencies..."

    local missing_deps=()

    # Check for required tools
    for cmd in gcc make pkg-config; do
        if ! command -v $cmd &> /dev/null; then
            missing_deps+=($cmd)
        else
            print_status $GREEN "✅ $cmd found"
        fi
    done

    # Check for libraries (Ubuntu/Debian style)
    if pkg-config --exists sqlite3; then
        print_status $GREEN "✅ libsqlite3-dev found"
    else
        missing_deps+=("libsqlite3-dev")
    fi

    if [ ${#missing_deps[@]} -gt 0 ]; then
        print_status $RED "❌ Missing dependencies: ${missing_deps[*]}"
        print_status $YELLOW "Install with: sudo apt-get install ${missing_deps[*]}"
        exit 1
    else
        print_status $GREEN "✅ All dependencies satisfied"
    fi
}

# Function to show help
show_help() {
    echo "Spear CI Helper Script"
    echo
    echo "Usage: $0 [command]"
    echo
    echo "Commands:"
    echo "  status     - Check recent workflow runs"
    echo "  cancel     - Cancel running workflows (requires GITHUB_TOKEN)"
    echo "  build      - Test local build"
    echo "  deps       - Check build dependencies"
    echo "  help       - Show this help message"
    echo
    echo "Environment variables:"
    echo "  GITHUB_TOKEN - Personal access token for GitHub API (for cancel command)"
    echo
    echo "Examples:"
    echo "  $0 status          # Check workflow status"
    echo "  $0 build           # Test build locally"
    echo "  GITHUB_TOKEN=... $0 cancel  # Cancel running workflows"
}

# Main script logic
case "${1:-status}" in
    "status"|"s")
        check_workflows
        ;;
    "cancel"|"c")
        if [ -z "$GITHUB_TOKEN" ]; then
            print_status $RED "❌ GITHUB_TOKEN environment variable required for cancel command"
            echo "Get a token at: https://github.com/settings/tokens"
            exit 1
        fi
        cancel_workflows
        ;;
    "build"|"b")
        check_dependencies
        test_local_build
        ;;
    "deps"|"d")
        check_dependencies
        ;;
    "help"|"h"|"-h"|"--help")
        show_help
        ;;
    *)
        print_status $RED "❌ Unknown command: $1"
        show_help
        exit 1
        ;;
esac
