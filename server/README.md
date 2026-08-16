# Switchboard server

Two pieces:

- **Asterisk** — the actual phone switch. Dialplan routes any 3–6 digit
  number starting 1–9, plus the service range (600 echo test, 601 playback).
  Endpoint definitions live in `asterisk/pjsip_devices.conf`.
- **Switchboard app** (`switchboard/`) — accounts, devices, and numbers, per
  the spec (`docs/provisioning-and-switchboard-spec.md`). Admins create users
  and approve number requests; users add devices and read their call logs; the
  boxes log in via its API to show status and the admin's contact email. It
  renders `pjsip_devices.conf` from its database and reloads Asterisk over AMI.

The app is optional for Milestone 0: `pjsip_devices.conf` ships with starter
extensions 101–103 so softphone testing works immediately.

## Docker (laptop/VPS/64-bit Pi)

```bash
docker compose up -d
# web UI:
open http://localhost:8080     # log in: admin / changeme-admin (CHANGE IT)
# verify registrations:
docker exec -it openphone-switchboard asterisk -rx "pjsip show contacts"
# watch RTP while debugging audio:
docker exec -it openphone-switchboard asterisk -rx "rtp set debug on"
```

Before exposing anything beyond your LAN: set `ADMIN_PASSWORD`,
`SWITCHBOARD_SECRET`, and the AMI secret (`asterisk/manager.conf` +
`AMI_PASS` in `docker-compose.yml`) — the defaults are placeholders.

## Native install (better for 32-bit Pis like the Pi 2)

```bash
sudo apt update && sudo apt install asterisk python3-flask
sudo cp asterisk/*.conf /etc/asterisk/
sudo systemctl restart asterisk
sudo asterisk -rx "pjsip show endpoints"

# switchboard app (writes /etc/asterisk/pjsip_devices.conf, so run as a user
# that can — or point ASTERISK_CONF_DIR somewhere asterisk includes):
cd switchboard
ASTERISK_CONF_DIR=/etc/asterisk python3 app.py
```

## How number assignment flows

1. Admin creates a user (admin panel → Users).
2. User logs in, adds a device — a SIP password is generated.
3. User requests a number (a specific free one, or "next available").
4. Admin approves → the app writes the endpoint into `pjsip_devices.conf`,
   reloads Asterisk, and the user's device page shows the SIP credentials to
   type into the box's config page.

Numbers are 3–6 digits, never starting with 0 (`0…` is reserved for future
switchboard-to-switchboard dialing) and never in `6XX` (service range).
