# Suigetsu C2

A command and control framework where the communication protocol is selected via cryptographic randomization. The implant and server independently derive the next protocol from a shared secret — no negotiation, no metadata.

---

## How It Works

Both implant and server share a 32-byte key embedded at build time. Each side independently computes:

```
counter  = unix_timestamp / window_size
protocol = ChaCha20(key, counter)[0] % pool_size
```

The protocol sequence is deterministic from the shared key but unpredictable to any observer. No handshake, no negotiation — both sides arrive at the same protocol from the clock alone. If a beacon is missed, the implant silently waits for the next window and derives a new protocol automatically.

Every beacon is signed with HMAC-SHA256. The server rejects any payload that fails verification.

**Protocol pool (v1):**

| Index | Protocol | Encoding |
|-------|----------|----------|
| 0 | HTTPS | Raw bytes in POST body |
| 1 | DNS | Base32 in TXT record subdomain |

---

## Project Structure

```
suigetsu/
├── server/
│   ├── main.go
│   ├── crypto/        — ChaCha20 protocol derivation, HMAC signing
│   ├── listeners/     — Listener manager, HTTPS and DNS handlers
│   ├── receiver/      — Beacon verification and parsing
│   ├── queue/         — SQLite task queue
│   └── shell/         — Operator interface
└── implant/
    ├── main.c
    ├── crypto/        — ChaCha20, counter, key deobfuscation
    ├── beacon/        — Beacon struct, serialize, parse
    ├── protocols/     — HTTPS and DNS handlers
    ├── executor/      — Task parsing and execution
    └── hmac/          — HMAC-SHA256 signing
```

---

## Build

### Server

Requires Go.

```bash
go mod tidy
go build -o suigetsu-server ./server
```

### Implant

Requires GCC. No external libraries needed on the target — everything is compiled in.

```bash
gcc -o implant \
    implant/main.c \
    implant/crypto/crypto.c \
    implant/beacon/beacon.c \
    implant/protocols/https.c \
    implant/protocols/dns.c \
    implant/executor/executor.c \
    implant/hmac/hmac.c
```

For Windows targets, cross-compile with MinGW:

```bash
x86_64-w64-mingw32-gcc -o implant.exe implant/main.c \
    implant/crypto/crypto.c \
    implant/beacon/beacon.c \
    implant/protocols/https.c \
    implant/protocols/dns.c \
    implant/executor/executor.c \
    implant/hmac/hmac.c \
    -lws2_32
```

---

## Usage

### Start the server

```bash
sudo ./suigetsu-server
```

> Root is required to bind to port 443 (HTTPS) and port 53 (DNS).

### Operator shell commands

| Command | Description |
|---------|-------------|
| `list` | Show all registered implants |
| `use <id>` | Select an implant by ID |
| `task <cmd>` | Queue a shell command for the selected implant |
| `results` | Show completed task output |
| `history` | Show all tasks and their states |
| `kill` | Queue a kill task for the selected implant |
| `export` | Dump results to a text file |
| `help` | Show command reference |

Task output is retrieved on the implant's next beacon. Results are stored persistently in `suigetsu.db`.

---

## Configuration

Set at build time in `implant/main.c` and `server/main.go`.

| Parameter | Location | Description |
|-----------|----------|-------------|
| `key` | Both | 32-byte shared secret. Must match on both sides. |
| `INTERVAL` | Implant | Base beacon interval in seconds. |
| `MAX_JITTER` | Implant | Maximum random seconds added to interval. |
| `WINDOW_SIZE` | Both | Window size in nanoseconds. Must be larger than `INTERVAL + MAX_JITTER`. |

Defaults: interval 60s, max jitter 30s, window 300s (300,000,000,000 ns).

> **The key in `main.go` is a placeholder. Replace it with a securely generated 32-byte value before any real use.**

---

## Disclaimer

This project is for **authorized security research and red team engagements only**. Deploying this software against systems you do not own or have explicit written permission to test is illegal. The authors accept no responsibility for misuse.
