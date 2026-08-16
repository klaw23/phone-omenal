// Milestone: provisioning. WiFi with captive-portal fallback + LAN config page.
// Spec: docs/provisioning-and-switchboard-spec.md §1.
//
// Behavior: try stored WiFi creds; after 5 failures (~45s) bring up an AP
// ("Phone-omenal-XXXX") with a captive portal at 192.168.4.1, while still
// retrying the real network in the background (APSTA). Once joined: tear the
// AP down, advertise mDNS "phone-omenal", serve the same config page on the
// LAN IP, log into the switchboard with the SIP creds and cache the admin
// email for display. Every read/write needs the device PIN. Stored secrets
// are never echoed back to a browser.
//
// Components needed in idf_component.yml / CMake: esp_wifi, esp_http_server,
// esp_http_client, nvs_flash, espressif/mdns, json (cJSON, bundled with IDF).
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "mdns.h"
#include "cJSON.h"

static const char *TAG = "provision";

#define MAX_JOIN_FAILS   5
#define AP_IP            "192.168.4.1"
#define API_PORT         8080          // switchboard app port

// ---------------------------------------------------------------- config (NVS)

typedef struct {
    char ssid[33], wifi_pass[65];
    char server[64];                   // switchboard host or IP
    char sip_user[16], sip_pass[64];   // sip_user is the phone number
    char pin[9];                       // device PIN; ships as printed sticker
} config_t;

static config_t cfg;
static char admin_email[64];           // fetched from switchboard after login
static char last_fail[64] = "not tried yet";
static bool wifi_up = false;

static void cfg_load(void) {
    nvs_handle_t h;
    if (nvs_open("phcfg", NVS_READONLY, &h) != ESP_OK) return;
    size_t n;
    #define GET(field) n = sizeof(cfg.field); nvs_get_str(h, #field, cfg.field, &n)
    GET(ssid); GET(wifi_pass); GET(server); GET(sip_user); GET(sip_pass); GET(pin);
    #undef GET
    nvs_close(h);
    if (!cfg.pin[0]) strcpy(cfg.pin, "0000");  // factory default, sticker overrides
}

static void cfg_save(void) {
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open("phcfg", NVS_READWRITE, &h));
    #define PUT(field) nvs_set_str(h, #field, cfg.field)
    PUT(ssid); PUT(wifi_pass); PUT(server); PUT(sip_user); PUT(sip_pass); PUT(pin);
    #undef PUT
    nvs_commit(h);
    nvs_close(h);
}

// ------------------------------------------------------------------- WiFi FSM

static int join_fails = 0;
static bool ap_mode = false;
static void portal_dns_start(void);

static void start_ap(void) {
    if (ap_mode) return;
    ap_mode = true;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    wifi_config_t ap = { 0 };
    snprintf((char *)ap.ap.ssid, 32, "Phone-omenal-%02X%02X", mac[4], mac[5]);
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_OPEN;   // page itself is PIN-gated
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));  // keep retrying STA
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    portal_dns_start();
    ESP_LOGI(TAG, "AP up: %s (portal at " AP_IP ")", ap.ap.ssid);
}

static void stop_ap(void) {
    if (!ap_mode) return;
    ap_mode = false;
    esp_wifi_set_mode(WIFI_MODE_STA);
    ESP_LOGI(TAG, "joined WiFi, AP torn down");
}

static void switchboard_login_task(void *arg);

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (cfg.ssid[0]) esp_wifi_connect();
        else { snprintf(last_fail, sizeof last_fail, "no WiFi configured"); start_ap(); }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_up = false;
        wifi_event_sta_disconnected_t *d = data;
        snprintf(last_fail, sizeof last_fail,
                 d->reason == WIFI_REASON_AUTH_FAIL ? "wrong password for \"%s\""
                                                    : "can't reach \"%s\" (reason %d)",
                 cfg.ssid, d->reason);
        if (++join_fails >= MAX_JOIN_FAILS) start_ap();
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_wifi_connect();            // keep trying forever, even in AP mode
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        wifi_up = true;
        join_fails = 0;
        stop_ap();
        mdns_init();
        mdns_hostname_set("phone-omenal");   // -> http://phone-omenal.local
        xTaskCreate(switchboard_login_task, "sb_login", 4096, NULL, 5, NULL);
    }
}

// ------------------------------------------------- captive DNS (AP mode only)
// Answer every A query with 192.168.4.1 so phones pop their "sign in" sheet.

static void dns_task(void *arg) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(53),
                             .sin_addr.s_addr = INADDR_ANY };
    bind(s, (struct sockaddr *)&a, sizeof a);
    uint8_t q[512];
    while (true) {
        struct sockaddr_in from; socklen_t fl = sizeof from;
        int n = recvfrom(s, q, sizeof q - 16, 0, (struct sockaddr *)&from, &fl);
        if (n < 12 || !ap_mode) continue;
        q[2] = 0x81; q[3] = 0x80;                       // response, no error
        q[6] = q[4]; q[7] = q[5];                       // ancount = qdcount
        // answer: pointer to name @12, A IN, TTL 60, 192.168.4.1
        uint8_t ans[] = { 0xc0, 12, 0, 1, 0, 1, 0, 0, 0, 60, 0, 4, 192, 168, 4, 1 };
        memcpy(q + n, ans, sizeof ans);
        sendto(s, q, n + sizeof ans, 0, (struct sockaddr *)&from, fl);
    }
}

static void portal_dns_start(void) {
    static bool started = false;
    if (!started) { started = true; xTaskCreate(dns_task, "dns", 3072, NULL, 4, NULL); }
}

// ------------------------------------------- switchboard API: fetch admin email

static void switchboard_login_task(void *arg) {
    if (!cfg.server[0] || !cfg.sip_user[0]) vTaskDelete(NULL);
    char url[128], body[192];
    snprintf(url, sizeof url, "http://%s:%d/api/login", cfg.server, API_PORT);
    snprintf(body, sizeof body, "{\"username\":\"%s\",\"password\":\"%s\"}",
             cfg.sip_user, cfg.sip_pass);
    esp_http_client_config_t hc = { .url = url, .method = HTTP_METHOD_POST,
                                    .timeout_ms = 5000 };
    esp_http_client_handle_t c = esp_http_client_init(&hc);
    esp_http_client_set_header(c, "Content-Type", "application/json");
    esp_http_client_set_post_field(c, body, strlen(body));
    if (esp_http_client_perform(c) == ESP_OK &&
        esp_http_client_get_status_code(c) == 200) {
        char resp[512];
        int n = esp_http_client_read_response(c, resp, sizeof resp - 1);
        if (n > 0) {
            resp[n] = 0;
            cJSON *j = cJSON_Parse(resp);
            cJSON *e = j ? cJSON_GetObjectItem(j, "admin_email") : NULL;
            if (cJSON_IsString(e))
                strlcpy(admin_email, e->valuestring, sizeof admin_email);
            cJSON_Delete(j);
            ESP_LOGI(TAG, "switchboard ok, admin: %s", admin_email);
        }
    } else {
        snprintf(admin_email, sizeof admin_email, "(switchboard unreachable)");
    }
    esp_http_client_cleanup(c);
    vTaskDelete(NULL);
}

// ------------------------------------------------------------ config web page
// Served identically in AP mode (portal) and on the LAN IP. PIN-gated.
// Secrets are write-only: the page shows "SSID: x (set)" but never passwords.

static const char PAGE[] =
"<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>Phone-omenal setup</title>"
"<style>body{font:16px system-ui;max-width:26em;margin:2em auto;padding:0 1em}"
"input{width:100%;padding:.5em;margin:.2em 0 .8em;box-sizing:border-box}"
"button{padding:.6em 1.2em}#st{background:#eee;padding:.8em;border-radius:.5em}</style>"
"<h1>&#128222; Phone-omenal</h1><div id=st>loading…</div>"
"<form id=f><h3>WiFi</h3><input name=ssid placeholder='network name'>"
"<input name=wifi_pass type=password placeholder='password (unchanged if blank)'>"
"<h3>Switchboard</h3><input name=server placeholder='server address'>"
"<input name=sip_user placeholder='phone number'>"
"<input name=sip_pass type=password placeholder='SIP password (unchanged if blank)'>"
"<h3>Device PIN</h3><input name=pin placeholder='PIN (required)' required>"
"<button>Save &amp; apply</button></form>"
"<script>"
"const pin=()=>document.querySelector('[name=pin]').value;"
"async function st(){try{const r=await fetch('/api/status?pin='+pin());"
"if(r.status==403){document.querySelector('#st').textContent='enter PIN, then Save or wait';return}"
"const j=await r.json();document.querySelector('#st').innerHTML="
"`WiFi: <b>${j.ssid||'(unset)'}</b> ${j.wifi_up?'&#10003; connected':'&#10007; '+j.last_fail}<br>"
"Number: <b>${j.sip_user||'(unset)'}</b> @ ${j.server||'(unset)'}<br>"
"Admin contact: <b>${j.admin_email||'—'}</b>`}catch(e){}}"
"setInterval(st,3000);st();"
"document.querySelector('#f').onsubmit=async e=>{e.preventDefault();"
"const d=Object.fromEntries(new FormData(e.target));"
"const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify(d)});alert(r.ok?'Saved — rebooting WiFi':'Wrong PIN');st()};"
"</script>";

static bool pin_ok(httpd_req_t *req) {
    char qs[64], pin[16] = "";
    if (httpd_req_get_url_query_str(req, qs, sizeof qs) == ESP_OK)
        httpd_query_key_value(qs, "pin", pin, sizeof pin);
    return strcmp(pin, cfg.pin) == 0;
}

static esp_err_t h_page(httpd_req_t *req) {
    return httpd_resp_send(req, PAGE, HTTPD_RESP_USE_STRLEN);
}

// captive-portal probes (Android/Apple/Windows) -> redirect to the page
static esp_err_t h_redirect(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://" AP_IP "/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t h_status(httpd_req_t *req) {
    if (!pin_ok(req)) { httpd_resp_set_status(req, "403 Forbidden");
                        return httpd_resp_send(req, "{}", 2); }
    char out[320];
    snprintf(out, sizeof out,
        "{\"ssid\":\"%s\",\"wifi_up\":%s,\"last_fail\":\"%s\","
        "\"server\":\"%s\",\"sip_user\":\"%s\",\"admin_email\":\"%s\"}",
        cfg.ssid, wifi_up ? "true" : "false", wifi_up ? "" : last_fail,
        cfg.server, cfg.sip_user, admin_email);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t h_config(httpd_req_t *req) {
    char body[512];
    int n = httpd_req_recv(req, body, sizeof body - 1);
    if (n <= 0) return ESP_FAIL;
    body[n] = 0;
    cJSON *j = cJSON_Parse(body);
    if (!j) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad json");
    const cJSON *pin = cJSON_GetObjectItem(j, "pin");
    if (!cJSON_IsString(pin) || strcmp(pin->valuestring, cfg.pin) != 0) {
        cJSON_Delete(j);
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_send(req, "wrong pin", HTTPD_RESP_USE_STRLEN);
    }
    // blank/missing fields mean "keep current value" (secrets aren't echoed,
    // so the form legitimately submits them empty)
    #define TAKE(field) do { const cJSON *v = cJSON_GetObjectItem(j, #field); \
        if (cJSON_IsString(v) && v->valuestring[0]) \
            strlcpy(cfg.field, v->valuestring, sizeof cfg.field); } while (0)
    TAKE(ssid); TAKE(wifi_pass); TAKE(server); TAKE(sip_user); TAKE(sip_pass);
    #undef TAKE
    cfg_save();
    cJSON_Delete(j);
    httpd_resp_send(req, "ok", HTTPD_RESP_USE_STRLEN);
    join_fails = 0;
    admin_email[0] = 0;
    esp_wifi_disconnect();             // triggers reconnect with new creds
    wifi_config_t sta = { 0 };
    strlcpy((char *)sta.sta.ssid, cfg.ssid, 32);
    strlcpy((char *)sta.sta.password, cfg.wifi_pass, 64);
    esp_wifi_set_config(WIFI_IF_STA, &sta);
    return ESP_OK;
}

static void web_start(void) {
    httpd_handle_t srv;
    httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
    hc.uri_match_fn = httpd_uri_match_wildcard;
    ESP_ERROR_CHECK(httpd_start(&srv, &hc));
    httpd_uri_t u1 = { .uri = "/",            .method = HTTP_GET,  .handler = h_page };
    httpd_uri_t u2 = { .uri = "/api/status",  .method = HTTP_GET,  .handler = h_status };
    httpd_uri_t u3 = { .uri = "/api/config",  .method = HTTP_POST, .handler = h_config };
    httpd_uri_t u4 = { .uri = "/*",           .method = HTTP_GET,  .handler = h_redirect };
    httpd_register_uri_handler(srv, &u1);
    httpd_register_uri_handler(srv, &u2);
    httpd_register_uri_handler(srv, &u3);
    httpd_register_uri_handler(srv, &u4);   // wildcard last: catches portal probes
}

// ------------------------------------------------------------------- app_main

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    cfg_load();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t wc = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wc));
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL);
    wifi_config_t sta = { 0 };
    strlcpy((char *)sta.sta.ssid, cfg.ssid, 32);
    strlcpy((char *)sta.sta.password, cfg.wifi_pass, 64);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    ESP_ERROR_CHECK(esp_wifi_start());
    web_start();
    // WiFi power save off — it causes dropouts mid-call (see build guide §11)
    esp_wifi_set_ps(WIFI_PS_NONE);
}
