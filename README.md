# esp32-wifi-prov

ESP-IDF component for first-boot WiFi provisioning via a captive portal. On first boot (or after a credential wipe) the device starts a SoftAP and serves a web form; the user connects, fills in credentials, and the device stores them in NVS and connects as a station. Subsequent boots skip the portal and connect directly.

Supports all major ESP32 variants (ESP32, S2, S3, C3, C6, H2, P4). Requires ESP-IDF ≥ 5.0.

## Features

- SoftAP + captive portal — iOS, Android, Windows, Firefox, and Chrome all auto-open the form
- NVS credential storage under namespace `"wifi_prov"`
- Arbitrary extra form fields (text, password, number, dropdown) stored alongside WiFi creds
- Non-secret fields pre-populate from NVS on re-entry
- `force_portal` flag: jump straight to the portal without wiping stored data (for app-level auth recovery)
- Boot GPIO: hold a GPIO for 3 s at boot to force re-provisioning (credentials preserved)
- `on_portal` / `on_connect_failed` callbacks for display/UI updates
- `device_settings`: background GPIO monitor — 3-second hold at any time fires a user callback

## Repository layout

```
wifi_prov.c / wifi_prov.h          — provisioning component
device_settings.c / device_settings.h — background button monitor
CMakeLists.txt
idf_component.yml
nordesems__esp-captive-portal/     — captive portal sub-component (bundled, v1.3.0)
```

The captive portal is bundled rather than pulled from the registry so the build works without network access to components.espressif.com.

## Adding to a project

Add as a git submodule at `components/wifi_prov/`:

```sh
git submodule add https://github.com/dmatking/esp32-wifi-prov.git components/wifi_prov
```

Because ESP-IDF does not discover components nested inside another component directory, add a `file(COPY)` call to your root `CMakeLists.txt` **before** `include(project.cmake)` to expose the bundled captive portal:

```cmake
file(COPY "${CMAKE_SOURCE_DIR}/components/wifi_prov/nordesems__esp-captive-portal"
     DESTINATION "${CMAKE_SOURCE_DIR}/components/")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my_project)
```

Add `components/nordesems__esp-captive-portal/` to `.gitignore` — it is generated at configure time and should not be tracked.

## Usage

```c
#include "wifi_prov.h"

// Optional extra fields stored in NVS alongside WiFi credentials
static const wifi_prov_option_t tz_options[] = {
    { "US Central", "CST6CDT,M3.2.0,M11.1.0" },
    { "US Eastern", "EST5EDT,M3.2.0,M11.1.0" },
    // ...
};

static const wifi_prov_field_t extra_fields[] = {
    { .key = "username",  .label = "Username",  .placeholder = "your name" },
    { .key = "api_token", .label = "API Token",  .secret = true },
    { .key = "count",     .label = "Item count", .input_type = "number",
      .input_min = "1",   .input_max = "100" },
    { .key = "tz",        .label = "Timezone",
      .options = tz_options, .option_count = 2 },
};

void app_main(void)
{
    wifi_prov_config_t cfg = {
        .ap_ssid      = "MyDevice-Setup",
        .ap_password  = NULL,          // open AP
        .boot_gpio    = 0,             // hold GPIO0 for 3 s to re-provision
        .on_portal    = my_portal_cb,  // update display when portal starts
        .extra_fields = extra_fields,
        .extra_count  = sizeof(extra_fields) / sizeof(extra_fields[0]),
    };

    if (!wifi_prov_start(&cfg)) {
        ESP_LOGE("app", "WiFi failed");
        return;
    }

    // Read credentials back from NVS after connecting
    char username[64] = { 0 };
    wifi_prov_get("username", username, sizeof(username));
}
```

## API

### `wifi_prov_start()`

```c
bool wifi_prov_start(const wifi_prov_config_t *cfg);
```

Attempts to connect using stored NVS credentials. Falls back to the captive portal if no credentials are stored, if the connection fails, or if `force_portal` is set. Blocks until connected. Returns `true` on success.

The caller is responsible for any chip-specific WiFi hardware init before calling this (e.g. `esp_hosted_init()` on ESP32-P4).

### `wifi_prov_get()`

```c
bool wifi_prov_get(const char *key, char *buf, size_t len);
```

Reads a value from NVS by key. Works for `"ssid"`, `"pass"`, and any extra field key. Returns `false` if the key is not found.

### `wifi_prov_reset()`

```c
void wifi_prov_reset(void);
```

Erases all credentials from NVS, forcing the provisioning portal on the next boot.

## `force_portal`

Setting `cfg.force_portal = true` sends the device straight to the portal without erasing NVS. Non-secret fields are pre-populated from saved values. Useful when an app-level authentication failure (e.g. an expired API token) needs re-entry without making the user re-enter their WiFi password.

## `device_settings`

A background GPIO monitor that fires a callback after a 3-second button hold, at any point during normal operation. Useful for re-entering the provisioning portal, showing a settings menu, or triggering a factory reset — whatever the app needs.

```c
#include "device_settings.h"
#include "wifi_prov.h"

static wifi_prov_config_t s_cfg = { /* ... */ };

static void enter_settings(void)
{
    // force_portal=true re-enters the portal without erasing NVS credentials
    s_cfg.force_portal = true;
    wifi_prov_start(&s_cfg);
    s_cfg.force_portal = false;
}

void app_main(void)
{
    wifi_prov_start(&s_cfg);

    // Watch GPIO0 (boot button); fire enter_settings on any 3-second hold
    device_settings_start(0, enter_settings);

    // ... rest of app
}
```

The callback runs inside the monitor task and may block. After the callback returns, the monitor waits 3 seconds before watching for another press (to avoid immediate re-triggering while the button is still held).

### `device_settings_start()`

```c
void device_settings_start(int gpio_num, device_settings_cb_t on_hold);
```

Configures `gpio_num` as input with internal pull-up and spawns a low-priority background task. Call once after `wifi_prov_start()`.

## License

MIT
