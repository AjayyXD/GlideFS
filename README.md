# GlideFS (local-hotspot edition)

GlideFS turns a shared folder into something that "just works" on a small
Linux LAN: one machine broadcasts a WiFi hotspot, shares a folder over
Samba, and shows a live dashboard of who's connected and what's locked.
Other machines join the hotspot and get the folder mounted like a normal
local directory.

This build implements the **local-hotspot-only** slice of the original
GlideFS SRS: no Tailscale/mesh/WAN failover yet (see "What's not in this
build" below) — everything runs over the access point's own subnet
(`192.168.42.0/24`), which is exactly what `hotspotctl` sets up.

```
  [Host laptop]───────────────hotspot (wlan0)───────────────
        │                                                    │
   samba share                                          [Client laptop]
   192.168.42.1                                          192.168.42.x
        │                                                    │
   dashboard :8090  <───────────────────────────────  browser / curl
```

## Components

| Piece         | What it does                                              |
|---------------|------------------------------------------------------------|
| `hotspotctl`  | Vendored dependency (bundled in `third_party/`). Brings up the WiFi access point (hostapd + dnsmasq + nftables). |
| `glidefsctl`  | The GlideFS orchestrator. Generates the Samba config, starts/stops `smbd`, drives `hotspotctl`, runs the dashboard, and handles client `connect`/`disconnect`. |
| Samba (`smbd`)| Does the actual file serving, permissions and oplock-based file locking. Must be installed separately (see below). |
| Dashboard     | A tiny built-in HTTP server (no external deps) at `http://192.168.42.1:8090` showing connected devices, throughput, active Samba sessions and locks. |

## Requirements (host)

- Linux with a WiFi adapter capable of AP (master) mode
- `hostapd`, `dnsmasq`, `nftables`, `iproute2` (hotspotctl's own deps)
- `samba` (for `smbd`)
- root privileges (hotspot creation, Samba, mounting are all privileged)

```bash
# Debian/Ubuntu
sudo apt install hostapd dnsmasq nftables iproute2 samba

# Arch
sudo pacman -S hostapd dnsmasq nftables iproute2 samba
```

## Requirements (client)

- `cifs-utils` (provides `mount.cifs`)
- NetworkManager (`nmcli`) to join the hotspot automatically — if you'd
  rather join the WiFi network yourself, connect manually and use
  `connect` only for the mount step (the mount will still work as long
  as you're on the `192.168.42.0/24` network).

```bash
sudo apt install cifs-utils network-manager
```

## Build

```bash
make            # builds bin/glidefsctl and bin/hotspotctl (vendored)
sudo make install   # installs both to /usr/local/bin
```

`glidefsctl` looks for `hotspotctl` next to itself first, then on `PATH`,
then in `/usr/local/bin` and `/usr/bin` — so `make` (without installing)
works too, since both binaries land in `bin/` together.

## Usage

### Host a folder

```bash
sudo glidefsctl share ~/Projects/design-assets -n design -s DesignTeam -p supersecret
```

- `-n <name>` (required) — the Samba share name
- `-s <ssid>` (optional) — defaults to `GlideFS-<name>`
- `-p <password>` (optional) — auto-generated (and printed) if omitted
- `-d` — debug mode, keeps hostapd/dnsmasq output on the console

This will:
1. Start the access point via `hotspotctl` (auto interface/band/channel detection)
2. Write `/run/glidefs/smb.conf` and start `smbd` against it
3. Start the dashboard on port 8090
4. Print the SSID/password and the exact `connect` command to give collaborators

### Check status / stop

```bash
glidefsctl status
sudo glidefsctl unshare
```

`unshare` stops `smbd`, the dashboard, and tears the hotspot back down —
cleanly, the same way `hotspotctl stop` would.

### Connect from another machine

```bash
sudo glidefsctl connect design -s DesignTeam -p supersecret
```

This joins the WiFi network via `nmcli`, waits for the host to answer,
then mounts `//192.168.42.1/design` at `~/GlideFS/design` (owned by your
user, not root). Pass `-m <path>` to mount somewhere else.

```bash
sudo glidefsctl disconnect design
```

### Dashboard

Once a share is active, open `http://192.168.42.1:8090` from any device
on the hotspot (or `http://localhost:8090` on the host itself). It polls
`/api/status` every 2 seconds and shows:

- SSID / share path / interface
- Live download & upload throughput
- Connected devices (from `ip neighbour`)
- Active Samba sessions and locks (from `smbstatus`)

## How locking works

Conflict prevention is handled natively by Samba's **oplocks** — no
GlideFS-specific logic. When one client has a file open, a second client
gets the OS's normal "file is locked" behavior instead of a
`file (conflicted copy).txt`. `smbstatus -L` (surfaced on the dashboard)
shows exactly which files are locked and by whom.

## What's not in this build

Per your instructions this build is **local-hotspot only**. The original
SRS also envisioned:

- Dual-transport automatic path selection (LAN vs. Tailscale mesh/WAN)
- Seamless mid-session failover between LAN and mesh
- Tailscale/Headscale-based NAT traversal for off-LAN collaborators

None of that is implemented here — everything assumes host and clients
are on the same hotspot subnet (`192.168.42.0/24`). Adding a WAN path
later would slot in as an alternate transport in `client.c`/`hotspot.c`
without touching the Samba/dashboard/locking logic.

## Project layout

```
glidefs/
├── Makefile                 top-level build (glidefsctl + vendored hotspotctl)
├── include/                 headers
├── src/
│   ├── main.c                entry point / command dispatch
│   ├── cli.c                 command implementations (init/share/unshare/status/connect/disconnect)
│   ├── hotspot.c              wraps hotspotctl start/stop
│   ├── share.c                generates smb.conf, starts/stops smbd
│   ├── dashboard.c            embedded HTTP server + JSON status API
│   ├── client.c                nmcli join + CIFS mount/umount
│   ├── state.c                 /run/glidefs/glidefs.state read/write
│   └── util.c                   logging, run_cmd, mkdir -p, root check
├── assets/dashboard.html      dashboard source (embedded into the binary at build time)
└── third_party/hotspotctl/    vendored hotspotctl dependency
```

## Troubleshooting

- **"hotspotctl not found"** — run `make` from the repo root (it builds
  the vendored copy) or `sudo make install`.
- **"smbd (Samba) not found"** — install the `samba` package.
- **Hotspot won't start** — check `/run/hotspotctl/hostapd.log`; most
  often this is a WiFi card that doesn't support AP/master mode, or
  NetworkManager still holding the interface (hotspotctl unmanages it
  automatically if `nmcli` is present).
- **Client can't join automatically** — `nmcli` is required for the
  automatic WiFi join step; without it, connect to the SSID manually and
  the mount step will still succeed once you're on `192.168.42.0/24`.
