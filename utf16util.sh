#!/bin/bash
# utf16util - Utility for working with UTF-16LE files (like mpc-hc.rc)
# Usage:
#   utf16util.sh <file> <command> [args...]
#   utf16util.sh mpc-hc.rc grep "pattern"
#   utf16util.sh mpc-hc.rc sed -i 's/old/new/'

set -e

if [ $# -lt 2 ]; then
    echo "Usage: $0 <utf16-file> <command> [args...]" >&2
    echo "Examples:" >&2
    echo "  $0 mpc-hc.rc grep 'IDD_FAVORGANIZE'" >&2
    echo "  $0 mpc-hc.rc sed -i '100s/old/new/'" >&2
    exit 1
fi

UTF16_FILE="$1"
shift
COMMAND="$1"
shift

# Check if file exists
if [ ! -f "$UTF16_FILE" ]; then
    echo "Error: File '$UTF16_FILE' not found" >&2
    exit 1
fi

# Create temp file
TMP_FILE=$(mktemp)
trap "rm -f $TMP_FILE" EXIT

# Convert UTF-16LE to UTF-8
iconv -f UTF-16LE -t UTF-8 "$UTF16_FILE" > "$TMP_FILE"

# Determine if this is a write operation
WRITE_BACK=false
case "$COMMAND" in
    sed)
        # Check if -i flag is present
        for arg in "$@"; do
            if [[ "$arg" == "-i" ]]; then
                WRITE_BACK=true
                break
            fi
        done
        ;;
    awk|perl)
        # Check if -i flag is present (for in-place editing)
        for arg in "$@"; do
            if [[ "$arg" == "-i" ]] || [[ "$arg" =~ ^-i ]]; then
                WRITE_BACK=true
                break
            fi
        done
        ;;
esac

# Execute command on the UTF-8 temp file
if [ "$WRITE_BACK" = true ]; then
    # For write operations, replace the file reference with temp file
    "$COMMAND" "$@" "$TMP_FILE"
    # Convert back to UTF-16LE
    iconv -f UTF-8 -t UTF-16LE "$TMP_FILE" > "$UTF16_FILE"
else
    # For read operations, just run and output
    "$COMMAND" "$@" "$TMP_FILE"
fi
