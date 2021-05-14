#include <freertos/FreeRTOS.h>

#include <esp_log.h>
#include <esp_sntp.h>
#include <nvs_flash.h>
#include <driver/gpio.h>

#include <wifi_prov_connect.h>

static const char *TAG = "sched_controlled_switch";

void app_main(void) {
	// Setup control pin and turn relay off
	ESP_ERROR_CHECK(gpio_reset_pin(CONFIG_CONTROL_PIN));
	ESP_ERROR_CHECK(gpio_set_direction(CONFIG_CONTROL_PIN, GPIO_MODE_OUTPUT));
	ESP_ERROR_CHECK(gpio_set_level(CONFIG_CONTROL_PIN, 0));

	// Initialize non-volatile storage
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_LOGW(TAG, "nvs_flash_init failed w/%s; erasing and reinitializing flash", esp_err_to_name(err));
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	// Connect to WiFi, provisioning connection if necessary
	ESP_ERROR_CHECK(wifi_prov_connect());

	// Set system timezone
	setenv("TZ", CONFIG_TZ, 1);
	tzset();

	// Start NTP client to set system time
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, "pool.ntp.org");
	ESP_LOGI(TAG, "Initializing NTP client");
	sntp_init();

	// Wait until time is set
	sntp_sync_status_t status;
	while ((status = sntp_get_sync_status()) != SNTP_SYNC_STATUS_COMPLETED) {
		ESP_LOGI(TAG, "Awaiting initial time sync via NTP (status now %d) ...", status);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
	char buf[128];
	size_t cursor = snprintf(buf, sizeof(buf), "Sync'd time via NTP: ");
	time_t now;
	time(&now);
	struct tm timeinfo;
	localtime_r(&now, &timeinfo);
	strftime(buf+cursor, sizeof(buf)-cursor, "%c", &timeinfo);
	ESP_LOGI(TAG, "%s", buf);

	// Main control loop - just a dumb poll for now.  I'm in a hurry ...
	int oldRelayState = -1;
	while (1) {
		time(&now);
		localtime_r(&now, &timeinfo);
		int nowHHMM = timeinfo.tm_hour * 100 + timeinfo.tm_min;
		int relayState = CONFIG_ON_TIME < nowHHMM && nowHHMM < CONFIG_OFF_TIME;
		if (relayState != oldRelayState) {
			ESP_LOGI(TAG, "At %04d setting relayState to %s (%d)", nowHHMM, relayState ? "ON": "OFF", relayState);
			ESP_ERROR_CHECK(gpio_set_level(CONFIG_CONTROL_PIN, relayState));
			oldRelayState = relayState;
		}
		vTaskDelay(60000 / portTICK_PERIOD_MS);
	}
}

