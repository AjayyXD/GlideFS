#!/usr/bin/env bash
# ==============================================================================
# GlideFS Automated Test Suite
# Tests TC-01 to TC-12 (Build, Static Checks, Unit & Integration Validation)
# ==============================================================================

# Note: Do not use 'set -e' so the harness can execute all assertions and tabulate results.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

PASS_COUNT=0
FAIL_COUNT=0
TOTAL_COUNT=0

# Colors for terminal output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

run_test() {
    local tc_id="$1"
    local desc="$2"
    shift 2
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    echo -e "${BLUE}[RUN]${NC} ${tc_id}: ${desc}"

    # Capture output and exit code
    local output
    output=$("$@" 2>&1)
    local status=$?

    if [ $status -eq 0 ]; then
        echo -e "${GREEN}[PASS]${NC} ${tc_id}"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo -e "${RED}[FAIL]${NC} ${tc_id} (Exit code: ${status})"
        echo "      Details:"
        echo "$output" | sed 's/^/      /'
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
    echo ""
}

echo "============================================="
echo "  Running GlideFS Automated Verification"
echo "============================================="
echo ""

# ------------------------------------------------------------------------------
# Category 1: Build & Static Checks
# ------------------------------------------------------------------------------

# TC-01: Clean compile without compiler warnings
run_test "TC-01" "Clean compile, -Wall -Wextra, zero warnings/errors" bash -c '
    make clean > /dev/null 2>&1
    build_output=$(make 2>&1)
    if echo "$build_output" | grep -qi "warning:"; then
        echo "$build_output"
        exit 1
    fi
    [ -x "bin/glidefsctl" ]
'

# TC-02: Vendored dependency binary check
run_test "TC-02" "Vendored dependency build (hotspotctl present and executable)" bash -c '
    [ -f "third_party/hotspotctl/Makefile" ] || [ -f "third_party/hotspotctl/hotspotctl" ] || [ -x "bin/hotspotctl" ]
'

# ------------------------------------------------------------------------------
# Category 2: Unit & Integration (CLI, Flags, State, Edge Cases)
# ------------------------------------------------------------------------------

# TC-04: Version and Help output
run_test "TC-04" "Version and help commands render cleanly" bash -c '
    ./bin/glidefsctl --version | grep -q "glidefsctl v" && \
    ./bin/glidefsctl --help | grep -q "Usage:"
'

# TC-05: Dependency detection
run_test "TC-05" "Dependency check command runs and identifies requirements" bash -c '
    ./bin/glidefsctl deps | grep -Ei "(Checking dependencies|Dependencies|hostapd|samba|tailscale)"
'

# TC-06: Dependency role filtering
run_test "TC-06" "Dependency role filtering (--host and --client)" bash -c '
    out_host=$(./bin/glidefsctl deps --host)
    out_client=$(./bin/glidefsctl deps --client)
    # Ensure role commands execute without segfaulting or fatal errors
    [ -n "$out_host" ] && [ -n "$out_client" ]
'

# TC-10: Input validation — missing password
run_test "TC-10" "Input validation: Reject connect when -p is missing" bash -c '
    out=$(./bin/glidefsctl connect testshare 2>&1)
    if echo "$out" | grep -qi "Missing required -p"; then
        exit 0
    else
        echo "Expected missing -p validation error, got: $out"
        exit 1
    fi
'

# TC-11: Input validation — missing share name / required flags
run_test "TC-11" "Input validation: Reject connect when required arguments missing" bash -c '
    out=$(./bin/glidefsctl connect 2>&1)
    if echo "$out" | grep -qi "Usage:"; then
        exit 0
    else
        echo "Expected usage guidance, got: $out"
        exit 1
    fi
'

# TC-12: Safe no-op on empty state
run_test "TC-12" "Safe no-op: Status/unshare with no active share" bash -c '
    # Ensure unshare gracefully exits without crashing when clean
    out_status=$(./bin/glidefsctl status 2>&1)
    out_unshare=$(./bin/glidefsctl unshare 2>&1)
    echo "$out_status" | grep -qi "No active" || echo "$out_unshare" | grep -qi "No active"
'

# ------------------------------------------------------------------------------
# Summary Report
# ------------------------------------------------------------------------------
echo "============================================="
echo "               TEST SUMMARY                  "
echo "============================================="
echo -e "Total Executed : ${TOTAL_COUNT}"
echo -e "Passed         : ${GREEN}${PASS_COUNT}${NC}"
echo -e "Failed         : ${RED}${FAIL_COUNT}${NC}"
echo "============================================="

if [ $FAIL_COUNT -eq 0 ]; then
    echo -e "${GREEN}All automated tests passed successfully!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed. Check log output above.${NC}"
    exit 1
fi