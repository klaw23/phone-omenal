// Milestone: BLE config — the always-on escape hatch.
// Spec: docs/provisioning-and-switchboard-spec.md §1.
//
// Companion to 05_provision.c: merge into the same project and make its
// cfg / cfg_save() / wifi_up / last_fail / admin_email non-static. This file
// adds a NimBLE GATT service mirroring the web page's fields, so a Web
// Bluetooth page (Chrome/Edge/Android — iOS has no Web Bluetooth; those users
// take the portal/LAN path) can configure the box even when WiFi is wedged.
//
// GATT model — one service, PIN-gated like the web page:
//   unlock   (write)  write the device PIN; unlocks this connection
//   ssid     (r/w)    WiFi network name
//   wifipass (write)  WiFi password        — write-only, never readable
//   server   (r/w)    switchboard address
//   sipuser  (r/w)    phone number
//   sippass  (write)  SIP password         — write-only, never readable
//   status   (read)   "up|<ssid>|<admin_email>" or "down|<ssid>|<last_fail>"
//   apply    (write)  save to NVS and reconnect WiFi
//
// Coexistence rule from the spec: advertising is always on, but defer GATT
// writes while a call is up — call ble_cfg_set_busy(true/false) from the call
// state machine. Use NimBLE (menuconfig: component config -> Bluetooth ->
// NimBLE), not Bluedroid — RAM.
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "ble_cfg";

// from 05_provision.c (made non-static)
typedef struct {
    char ssid[33], wifi_pass[65];
    char server[64];
    char sip_user[16], sip_pass[64];
    char pin[9];
} config_t;
extern config_t cfg;
extern char admin_email[64], last_fail[64];
extern bool wifi_up;
extern void cfg_save(void);

static bool unlocked = false;   // per-connection; reset on disconnect
static bool in_call = false;
void ble_cfg_set_busy(bool busy) { in_call = busy; }

// 128-bit UUIDs: base f0-ph-0me-... service ...0000, characteristics ...0001+
#define CFG_UUID(n) BLE_UUID128_INIT(0xf0, 0x9e, 0x0e, 0x7a, 0x6d, 0x3a, 0x4b, 0x21, \
                                     0x9c, 0x55, 0x70, 0x68, 0x6f, 0x6e, 0x00, (n))
static const ble_uuid128_t uuid_svc     = CFG_UUID(0x00);
static const ble_uuid128_t uuid_unlock  = CFG_UUID(0x01);
static const ble_uuid128_t uuid_ssid    = CFG_UUID(0x02);
static const ble_uuid128_t uuid_wpass   = CFG_UUID(0x03);
static const ble_uuid128_t uuid_server  = CFG_UUID(0x04);
static const ble_uuid128_t uuid_sipuser = CFG_UUID(0x05);
static const ble_uuid128_t uuid_sippass = CFG_UUID(0x06);
static const ble_uuid128_t uuid_status  = CFG_UUID(0x07);
static const ble_uuid128_t uuid_apply   = CFG_UUID(0x08);

static int flat(struct ble_gatt_access_ctxt *c, char *dst, size_t cap) {
    uint16_t n = 0;
    if (ble_hs_mbuf_to_flat(c->om, dst, cap - 1, &n) != 0) return -1;
    dst[n] = 0;
    return n;
}

static int access_cb(uint16_t conn, uint16_t attr,
                     struct ble_gatt_access_ctxt *c, void *arg) {
    const ble_uuid_t *u = c->chr->uuid;
    char buf[128];

    if (c->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (in_call) return BLE_ATT_ERR_WRITE_NOT_PERMITTED;    // defer: radio busy
        if (flat(c, buf, sizeof buf) < 0) return BLE_ATT_ERR_UNLIKELY;

        if (ble_uuid_cmp(u, &uuid_unlock.u) == 0) {
            unlocked = strcmp(buf, cfg.pin) == 0;
            ESP_LOGI(TAG, "unlock %s", unlocked ? "ok" : "FAILED");
            return unlocked ? 0 : BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
        }
        if (!unlocked) return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;

        if      (ble_uuid_cmp(u, &uuid_ssid.u)    == 0) strlcpy(cfg.ssid, buf, sizeof cfg.ssid);
        else if (ble_uuid_cmp(u, &uuid_wpass.u)   == 0) strlcpy(cfg.wifi_pass, buf, sizeof cfg.wifi_pass);
        else if (ble_uuid_cmp(u, &uuid_server.u)  == 0) strlcpy(cfg.server, buf, sizeof cfg.server);
        else if (ble_uuid_cmp(u, &uuid_sipuser.u) == 0) strlcpy(cfg.sip_user, buf, sizeof cfg.sip_user);
        else if (ble_uuid_cmp(u, &uuid_sippass.u) == 0) strlcpy(cfg.sip_pass, buf, sizeof cfg.sip_pass);
        else if (ble_uuid_cmp(u, &uuid_apply.u)   == 0) {
            cfg_save();
            wifi_config_t sta = { 0 };
            strlcpy((char *)sta.sta.ssid, cfg.ssid, 32);
            strlcpy((char *)sta.sta.password, cfg.wifi_pass, 64);
            esp_wifi_set_config(WIFI_IF_STA, &sta);
            esp_wifi_disconnect();     // reconnects with new creds (05's handler)
        }
        return 0;
    }

    if (c->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        if (!unlocked) return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
        if      (ble_uuid_cmp(u, &uuid_ssid.u)    == 0) strlcpy(buf, cfg.ssid, sizeof buf);
        else if (ble_uuid_cmp(u, &uuid_server.u)  == 0) strlcpy(buf, cfg.server, sizeof buf);
        else if (ble_uuid_cmp(u, &uuid_sipuser.u) == 0) strlcpy(buf, cfg.sip_user, sizeof buf);
        else if (ble_uuid_cmp(u, &uuid_status.u)  == 0)
            snprintf(buf, sizeof buf, "%s|%s|%s", wifi_up ? "up" : "down",
                     cfg.ssid, wifi_up ? admin_email : last_fail);
        else return BLE_ATT_ERR_READ_NOT_PERMITTED;   // secrets are write-only
        return os_mbuf_append(c->om, buf, strlen(buf)) ? BLE_ATT_ERR_INSUFFICIENT_RES : 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

#define CHR(uu, fl) { .uuid = &(uu).u, .access_cb = access_cb, .flags = (fl) }
static const struct ble_gatt_svc_def svcs[] = {
    { .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &uuid_svc.u,
      .characteristics = (struct ble_gatt_chr_def[]) {
          CHR(uuid_unlock,  BLE_GATT_CHR_F_WRITE),
          CHR(uuid_ssid,    BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE),
          CHR(uuid_wpass,   BLE_GATT_CHR_F_WRITE),
          CHR(uuid_server,  BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE),
          CHR(uuid_sipuser, BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE),
          CHR(uuid_sippass, BLE_GATT_CHR_F_WRITE),
          CHR(uuid_status,  BLE_GATT_CHR_F_READ),
          CHR(uuid_apply,   BLE_GATT_CHR_F_WRITE),
          { 0 } } },
    { 0 }
};

static void advertise(void);

static int gap_event(struct ble_gap_event *ev, void *arg) {
    switch (ev->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (ev->connect.status != 0) advertise();
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        unlocked = false;              // PIN unlock is per-connection
        advertise();
        return 0;
    }
    return 0;
}

static void advertise(void) {
    struct ble_hs_adv_fields f = { 0 };
    const char *name = ble_svc_gap_device_name();
    f.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    f.name = (uint8_t *)name;
    f.name_len = strlen(name);
    f.name_is_complete = 1;
    ble_gap_adv_set_fields(&f);
    struct ble_gap_adv_params p = { .conn_mode = BLE_GAP_CONN_MODE_UND,
                                    .disc_mode = BLE_GAP_DISC_MODE_GEN };
    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &p, gap_event, NULL);
}

static void on_sync(void) { advertise(); }
static void host_task(void *arg) { nimble_port_run(); }

void ble_cfg_start(void) {              // call from app_main after cfg_load()
    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.sync_cb = on_sync;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set("Phone-omenal");
    ESP_ERROR_CHECK(ble_gatts_count_cfg(svcs));
    ESP_ERROR_CHECK(ble_gatts_add_svcs(svcs));
    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG, "BLE config service advertising");
}
