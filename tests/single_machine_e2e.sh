#!/usr/bin/env bash
# ==============================================================================
# GlideFS Single-Machine E2E Automator
# Spins up the host, mounts as a client, verifies locks, and cleanly tears down.
# ==============================================================================

SHARE_DIR="/tmp/glidefs_test_src"
SHARE_NAME="autotest"
PASSWORD="strongpass123"
LOCAL_HOST_IP="192.168.42.1"
MOUNT_DIR="$HOME/GlideFS/$SHARE_NAME"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo "[*] Initializing Single-Machine E2E Environment..."
mkdir -p "$SHARE_DIR"
sudo glidefsctl unshare >/dev/null 2>&1 # Ensure clean state

# 1. Spin up the Host (in the background)
echo "-> Starting GlideFS Host..."
sudo glidefsctl share "$SHARE_DIR" -n $SHARE_NAME -p $PASSWORD > /tmp/glidefs_host.log 2>&1 &
HOST_PID=$!

# Wait for host to bind (checking the dashboard port)
sleep 4
if ! curl -s http://$LOCAL_HOST_IP:8090 > /dev/null; then
    echo -e "${RED}[FAIL] Host failed to start. Check /tmp/glidefs_host.log${NC}"
    exit 1
fi
echo -e "${GREEN}[PASS] Host is live on $LOCAL_HOST_IP.${NC}"

# 2. Spin up the Client (Self-Mount)
# We use -t to bypass nmcli Wi-Fi join and mount directly to the local hotspot IP
echo "-> Connecting Client (Self-Mount)..."
sudo glidefsctl connect $SHARE_NAME -p $PASSWORD -t $LOCAL_HOST_IP > /dev/null 2>&1

if ! mount | grep -q "$SHARE_NAME"; then
    echo -e "${RED}[FAIL] Client mount failed.${NC}"
    sudo glidefsctl unshare >/dev/null 2>&1
    exit 1
fi
echo -e "${GREEN}[PASS] Client mounted successfully at $MOUNT_DIR.${NC}"

# 3. Verify Bidirectional File Sync
echo "-> Testing File Sync..."
echo "automated_e2e_test" > "$MOUNT_DIR/sync_test.txt"
if grep -q "automated_e2e_test" "$SHARE_DIR/sync_test.txt"; then
    echo -e "${GREEN}[PASS] File written to client appeared in host directory.${NC}"
else
    echo -e "${RED}[FAIL] File sync failed.${NC}"
fi

# 4. Verify Native Samba Oplocks (TC-16)
echo "-> Testing Concurrent File Locks..."
# Hold a lock open on the file via the client mount
exec 3<> "$MOUNT_DIR/sync_test.txt"

# Try to overwrite the same file directly on the host file system
if bash -c "echo 'conflict' > $SHARE_DIR/sync_test.txt" 2>/dev/null; then
    echo -e "${RED}[FAIL] Lock test failed! Host was able to overwrite a locked client file.${NC}"
else
    echo -e "${GREEN}[PASS] Samba successfully prevented concurrent overwrite.${NC}"
fi
# Release the lock
exec 3>&-

# 5. Clean Teardown
echo "-> Tearing down environment..."
sudo glidefsctl disconnect $SHARE_NAME > /dev/null 2>&1
sudo glidefsctl unshare > /dev/null 2>&1

if mount | grep -q "$SHARE_NAME"; then
    echo -e "${RED}[FAIL] Client mount was not cleaned up.${NC}"
else
    echo -e "${GREEN}[PASS] Clean disconnect successful.${NC}"
fi

if kill -0 $HOST_PID 2>/dev/null; then
    echo -e "${RED}[FAIL] Host process failed to exit.${NC}"
else
    echo -e "${GREEN}[PASS] Host tore down cleanly.${NC}"
fi

echo ""
echo -e "${GREEN}[*] Single-Machine E2E Suite Complete!${NC}"