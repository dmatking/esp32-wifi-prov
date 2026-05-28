#pragma once

// Callback fired when the GPIO has been held for 3 seconds.
// Runs in the monitor task's context — may block (e.g. to call wifi_prov_start).
typedef void (*device_settings_cb_t)(void);

// Spawn a background task that watches gpio_num (active-low, internal pull-up).
// Fires on_hold after a 3-second continuous press. Safe to call once after boot.
void device_settings_start(int gpio_num, device_settings_cb_t on_hold);
