#!/bin/bash

# GitHub Actions Version Checker
# Checks for deprecated action versions in workflow files

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${NC}"
}

# Current recommended versions (update as needed)
declare -A RECOMMENDED_VERSIONS=(
    ["actions/checkout"]="v4"
    ["actions/cache"]="v4"
    ["actions/upload-artifact"]="v4"
    ["actions/download-artifact"]="v4"
    ["actions/setup-node"]="v4"
    ["actions/setup-python"]="v3"
    ["actions/setup-java"]="v4"
    ["github/super-linter"]="v5"
)

# Deprecated versions to flag
declare -A DEPRECATED_VERSIONS=(
    ["v1"]="Deprecated - security and maintenance issues"
    ["v2"]="Deprecated - may have security vulnerabilities"
    ["v3"]="Recently deprecated - update recommended"
)

print_status $BLUE "🔍 Checking GitHub Actions versions in workflow files..."

# Find all workflow files
workflow_files=$(find .github/workflows -name "*.yml" -o -name "*.yaml" 2>/dev/null || echo "")

if [ -z "$workflow_files" ]; then
    print_status $YELLOW "⚠️ No workflow files found in .github/workflows/"
    exit 0
fi

issues_found=0
total_actions=0

for file in $workflow_files; do
    print_status $BLUE "\n📄 Checking $file"

    # Extract action references using grep and sed
    actions=$(grep -n "uses:" "$file" | grep -v "^[[:space:]]*#" || true)

    if [ -z "$actions" ]; then
        print_status $YELLOW "   No actions found"
        continue
    fi

    while IFS= read -r line; do
        if [ -z "$line" ]; then continue; fi

        line_number=$(echo "$line" | cut -d: -f1)
        action_full=$(echo "$line" | sed -n 's/.*uses:[[:space:]]*\([^[:space:]]*\).*/\1/p' | tr -d '"'"'"'')

        if [ -z "$action_full" ]; then continue; fi

        total_actions=$((total_actions + 1))

        # Parse action name and version
        action_name=$(echo "$action_full" | cut -d@ -f1)
        action_version=$(echo "$action_full" | cut -d@ -f2)

        # Skip local actions (starting with ./)
        if [[ "$action_full" == ./* ]]; then
            continue
        fi

        print_status $NC "   Line $line_number: $action_full"

        # Check if version is deprecated
        deprecated_reason=""
        for dep_version in "${!DEPRECATED_VERSIONS[@]}"; do
            if [[ "$action_version" == $dep_version* ]]; then
                deprecated_reason="${DEPRECATED_VERSIONS[$dep_version]}"
                break
            fi
        done

        if [ -n "$deprecated_reason" ]; then
            print_status $RED "   ❌ DEPRECATED: $deprecated_reason"
            issues_found=$((issues_found + 1))

            # Suggest replacement if we have a recommendation
            if [[ -v RECOMMENDED_VERSIONS["$action_name"] ]]; then
                recommended="${RECOMMENDED_VERSIONS["$action_name"]}"
                print_status $YELLOW "   💡 Recommended: $action_name@$recommended"
            fi
        else
            # Check if we have a newer recommendation
            if [[ -v RECOMMENDED_VERSIONS["$action_name"] ]]; then
                recommended="${RECOMMENDED_VERSIONS["$action_name"]}"
                if [ "$action_version" != "$recommended" ]; then
                    print_status $YELLOW "   ℹ️ Newer version available: $action_name@$recommended"
                else
                    print_status $GREEN "   ✅ Using recommended version"
                fi
            else
                print_status $GREEN "   ✅ Version looks good"
            fi
        fi

    done <<< "$actions"
done

print_status $BLUE "\n📊 Summary:"
print_status $NC "   Total actions checked: $total_actions"

if [ $issues_found -eq 0 ]; then
    print_status $GREEN "   ✅ No deprecated actions found!"
else
    print_status $RED "   ❌ Found $issues_found deprecated action(s)"
    print_status $YELLOW "   📝 Consider updating these actions to avoid CI failures"
fi

print_status $BLUE "\n🔧 Quick fixes:"
print_status $NC "   # Find and replace deprecated versions"
print_status $NC "   sed -i 's/actions\\/checkout@v3/actions\\/checkout@v4/g' .github/workflows/*.yml"
print_status $NC "   sed -i 's/actions\\/cache@v3/actions\\/cache@v4/g' .github/workflows/*.yml"
print_status $NC "   sed -i 's/actions\\/upload-artifact@v3/actions\\/upload-artifact@v4/g' .github/workflows/*.yml"

print_status $BLUE "\n📚 Resources:"
print_status $NC "   • GitHub Actions marketplace: https://github.com/marketplace?type=actions"
print_status $NC "   • Changelog: https://github.blog/changelog/"
print_status $NC "   • Action versions: https://github.com/actions"

if [ $issues_found -gt 0 ]; then
    exit 1
else
    exit 0
fi
