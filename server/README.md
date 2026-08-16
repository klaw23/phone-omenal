# Switchboard server

Minimal Asterisk switchboard: three extensions (101–103), an echo test (600),
and a playback test (601). See `docs/esp32-build-guide.md` Milestone 0.

```bash
docker compose up -d
# verify registrations:
docker exec -it openphone-switchboard asterisk -rx "pjsip show contacts"
# watch RTP while debugging audio:
docker exec -it openphone-switchboard asterisk -rx "rtp set debug on"
```

Change the passwords in `asterisk/pjsip.conf` before exposing this to the
internet. The long-term plan (see the main README) is a web app that manages
these configs — device claiming, number assignment, per-phone allowlists —
writing to Asterisk's database backend instead of flat files.
