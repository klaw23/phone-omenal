# Phone-omenal — provisioning & switchboard spec

How a box gets configured, and how the switchboard manages people, devices,
and numbers. Firmware reference code: `firmware/examples/05_provision.c` and
`06_ble_config.c`. Server implementation: `server/switchboard/`.

## 1. Box configuration

The box stores five settings (in NVS flash): WiFi SSID, WiFi password,
switchboard address, SIP username (= its phone number), SIP password.
Plus a 4–8 digit **device PIN** (printed on a sticker inside the lid) that
gates every config surface.

Three ways in, one rule each:

| Surface | Available when | Why |
|---|---|---|
| **Captive portal** (box's own AP) | WiFi is failing: no stored credentials, or ~45s / 5 straight failed joins | First-time setup and "I typed the wrong password" recovery |
| **LAN config page** (`http://phone-omenal.local`) | Box is on WiFi | Everyday reconfiguration; the switchboard portal deep-links here per device |
| **BLE web config** (Web Bluetooth GATT) | Always advertising | Universal escape hatch — works even when WiFi is wedged. Chrome/Edge/Android only (no iOS Web Bluetooth); the portal covers everyone else |

State machine:

```
            boot
             │
      creds stored? ──no──► AP MODE: SSID "Phone-omenal-XXXX",
             │yes           captive DNS, portal at 192.168.4.1
             ▼                  │ creds submitted
        JOIN WiFi ◄─────────────┘
        │        │
     joined    5 fails / 45s
        │        └──────────► AP MODE (portal shows last failure reason;
        ▼                     keeps retrying STA in the background — APSTA)
   CONNECTED: LAN page up, mDNS "phone-omenal",
   SIP registers, AP torn down
                                     BLE: advertising in every state
```

Rules:

- **Portal never coexists with calls** by construction: no WiFi → no SIP.
- The portal must display the last join failure ("wrong password for
  `MyWifi`", "no IP from DHCP") or users retry the same typo forever.
- **Auth**: every surface requires the device PIN before reading or writing
  anything. BLE additionally uses LE pairing. The LAN page is plain HTTP
  (TLS with a self-signed cert on an ESP32 only trains users to click through
  scare screens), so: LAN-only, PIN required, and **stored secrets are never
  echoed back** — the page shows `SSID: MyWifi ✓` and password fields stay
  blank.
- **Coexistence**: BLE advertises always, but GATT writes are deferred while
  a call is up (one 2.4GHz radio, time-sliced). Firmware uses NimBLE, not
  Bluedroid — RAM.
- After the box registers, it logs into the switchboard API with its SIP
  credentials and displays the **switchboard admin's email** on all three
  config surfaces ("problems? contact …").

## 2. Numbering plan

- A number is a dialable extension: digits only, 3–6 digits.
- **First digit is never 0.** The entire `0…` space is reserved as the
  inter-switchboard prefix for future federation (dial `0` + switchboard +
  number). The dialplan hard-reserves `_0.` so nothing can ever squat on it.
- **`6XX` is the service range** (600 echo test, 601 playback, future
  voicemail etc.) — not assignable to devices.
- Everything else (`100`–`599`, `700`–`999`, and longer) is assignable.
- One number ↔ one device. The number **is** the device's SIP username; a
  device without a number can't register (there'd be nothing to reach it at).

## 3. Switchboard roles

**Admin** (bootstrap account, has an email address — the one shown on boxes):

- create/disable user accounts
- create devices for any user, assign/change/release numbers directly
- approve or reject number requests
- see every device, registration status, and call log

**User** (created by an admin):

- add devices (each gets a generated SIP password, shown once)
- request a number for a device — a specific free one or "next available";
  assignment happens when an admin approves
- see their devices: number, registration status (from Asterisk), SIP
  credentials, and per-device call logs (from CDR)

## 4. Server architecture

```
Flask app (server/switchboard, SQLite) ──writes──► asterisk/pjsip_devices.conf
        │                                            (endpoint per number)
        ├──AMI :5038──► Asterisk  ("pjsip reload", "pjsip show contacts")
        └──reads──── /var/log/asterisk/cdr-csv/Master.csv  (call logs)
```

- The app owns `pjsip_devices.conf` (one `[number]` endpoint per assigned
  device, rendered from the DB) and reloads Asterisk over AMI after every
  change. `pjsip.conf` keeps the transport + templates and `#include`s it.
  The static dialplan needs no regeneration: `_[1-9]XX` (and longer
  patterns) already route any legal number.
- Registration status comes from parsing `pjsip show contacts` over AMI;
  call logs from the CDR CSV, filtered to rows where the device's number is
  caller or callee.
- Pi 2 friendly: Flask + SQLite + raw-socket AMI, no other dependencies.

## 5. HTTP API (used by the box and the BLE/portal pages)

| Endpoint | Auth | Returns |
|---|---|---|
| `POST /api/login` `{username, password}` | SIP creds (number + password) | `{token, number, owner, admin_email}` |
| `GET /api/me` | `Authorization: Bearer <token>` | same, minus token — the box polls this to show status + admin email |

Tokens are HMAC-signed, 24h expiry, no server-side session state.
