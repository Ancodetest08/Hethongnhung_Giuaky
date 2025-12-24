#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_RED_GPIO      3
#define LED_YELLOW_GPIO   2
#define LED_GREEN_GPIO    1
#define BUTTON_EMERG_GPIO 5
#define BUTTON_NIGHT_GPIO 6
#define DEBOUNCE_TIME_MS  50

// Enum cho sự kiện
typedef enum {
    EMERGENCY_ON,
    EMERGENCY_OFF,
    NIGHT_ON,
    NIGHT_OFF
} EventType_t;

// Enum cho trạng thái đèn giao thông
typedef enum {
    TRAFFIC_RED,
    TRAFFIC_YELLOW,
    TRAFFIC_GREEN
} TrafficState_t;

// Queue riêng cho từng chế độ
QueueHandle_t eventQueueEmergency;
QueueHandle_t eventQueueNight;

// Mutex bảo vệ flags
SemaphoreHandle_t flagMutex;

// Flags và countdown
volatile bool isEmergency = false;
volatile bool isNightMode = false;
volatile int emergencyCountdown = 0;
volatile int nightCountdown = 0;
volatile int trafficCountdown = 0;
volatile TrafficState_t currentTrafficState = TRAFFIC_RED;

static const char *TAG = "TrafficLightMini";

// ===== GPIO =====
void init_gpio() {
    gpio_set_direction(LED_RED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_YELLOW_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_GREEN_GPIO, GPIO_MODE_OUTPUT);

    gpio_set_direction(BUTTON_EMERG_GPIO, GPIO_MODE_INPUT);
    gpio_set_direction(BUTTON_NIGHT_GPIO, GPIO_MODE_INPUT);
    gpio_pullup_en(BUTTON_EMERG_GPIO);
    gpio_pullup_en(BUTTON_NIGHT_GPIO);
}

void turn_off_all_leds() {
    gpio_set_level(LED_RED_GPIO, 0);
    gpio_set_level(LED_YELLOW_GPIO, 0);
    gpio_set_level(LED_GREEN_GPIO, 0);
}

// ===== Task: Button =====
void ButtonTask(void *pvParameters) {
    bool prevEmerg = true;
    bool prevNight = true;
    TickType_t lastDebounceEmerg = 0;
    TickType_t lastDebounceNight = 0;

    while (1) {
        bool currEmerg = gpio_get_level(BUTTON_EMERG_GPIO);
        bool currNight = gpio_get_level(BUTTON_NIGHT_GPIO);
        TickType_t now = xTaskGetTickCount();

        // Emergency button
        if ((now - lastDebounceEmerg) > pdMS_TO_TICKS(DEBOUNCE_TIME_MS)) {
            if (prevEmerg && !currEmerg) {
                EventType_t event;
                xSemaphoreTake(flagMutex, portMAX_DELAY);
                event = isEmergency ? EMERGENCY_OFF : EMERGENCY_ON;
                xSemaphoreGive(flagMutex);

                xQueueSend(eventQueueEmergency, &event, 0);
                ESP_LOGI(TAG, "Sent %s", event == EMERGENCY_ON ? "EMERGENCY_ON" : "EMERGENCY_OFF");

                lastDebounceEmerg = now;
            }
            prevEmerg = currEmerg;
        }

        // Night button
        if ((now - lastDebounceNight) > pdMS_TO_TICKS(DEBOUNCE_TIME_MS)) {
            if (prevNight && !currNight) {
                EventType_t event;
                xSemaphoreTake(flagMutex, portMAX_DELAY);
                event = isNightMode ? NIGHT_OFF : NIGHT_ON;
                xSemaphoreGive(flagMutex);

                xQueueSend(eventQueueNight, &event, 0);
                ESP_LOGI(TAG, "Sent %s", event == NIGHT_ON ? "NIGHT_ON" : "NIGHT_OFF");

                lastDebounceNight = now;
            }
            prevNight = currNight;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ===== Task: Traffic Light =====
void TrafficLightTask(void *pvParameters) {
    while (1) {
        xSemaphoreTake(flagMutex, portMAX_DELAY);
        bool checkEmergency = isEmergency;
        bool checkNight = isNightMode;
        xSemaphoreGive(flagMutex);

        if (checkEmergency || checkNight) {
            xSemaphoreTake(flagMutex, portMAX_DELAY);
            trafficCountdown = 0;
            xSemaphoreGive(flagMutex);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // RED
        xSemaphoreTake(flagMutex, portMAX_DELAY);
        currentTrafficState = TRAFFIC_RED;
        trafficCountdown = 5;
        xSemaphoreGive(flagMutex);
        turn_off_all_leds();
        gpio_set_level(LED_RED_GPIO, 1);
        for (int i = 0; i < 5; i++) {
            xSemaphoreTake(flagMutex, portMAX_DELAY);
            if (isEmergency || isNightMode) {
                trafficCountdown = 0;
                xSemaphoreGive(flagMutex);
                turn_off_all_leds();
                break;
            }
            trafficCountdown--;
            xSemaphoreGive(flagMutex);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        // Kiểm tra lại sau RED
        xSemaphoreTake(flagMutex, portMAX_DELAY);
        checkEmergency = isEmergency;
        checkNight = isNightMode;
        xSemaphoreGive(flagMutex);
        if (checkEmergency || checkNight) continue;

        // YELLOW (sau RED)
        xSemaphoreTake(flagMutex, portMAX_DELAY);
        currentTrafficState = TRAFFIC_YELLOW;
        trafficCountdown = 2;
        xSemaphoreGive(flagMutex);
        turn_off_all_leds();
        gpio_set_level(LED_YELLOW_GPIO, 1);
        for (int i = 0; i < 2; i++) {
            xSemaphoreTake(flagMutex, portMAX_DELAY);
            if (isEmergency || isNightMode) {
                trafficCountdown = 0;
                xSemaphoreGive(flagMutex);
                turn_off_all_leds();
                break;
            }
            trafficCountdown--;
            xSemaphoreGive(flagMutex);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        // Kiểm tra lại sau YELLOW
        xSemaphoreTake(flagMutex, portMAX_DELAY);
        checkEmergency = isEmergency;
        checkNight = isNightMode;
        xSemaphoreGive(flagMutex);
        if (checkEmergency || checkNight) continue;

        // GREEN
        xSemaphoreTake(flagMutex, portMAX_DELAY);
        currentTrafficState = TRAFFIC_GREEN;
        trafficCountdown = 5;
        xSemaphoreGive(flagMutex);
        turn_off_all_leds();
        gpio_set_level(LED_GREEN_GPIO, 1);
        for (int i = 0; i < 5; i++) {
            xSemaphoreTake(flagMutex, portMAX_DELAY);
            if (isEmergency || isNightMode) {
                trafficCountdown = 0;
                xSemaphoreGive(flagMutex);
                turn_off_all_leds();
                break;
            }
            trafficCountdown--;
            xSemaphoreGive(flagMutex);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        // Kiểm tra lại sau GREEN
        xSemaphoreTake(flagMutex, portMAX_DELAY);
        checkEmergency = isEmergency;
        checkNight = isNightMode;
        xSemaphoreGive(flagMutex);
        if (checkEmergency || checkNight) continue;

        // YELLOW (sau GREEN, trước RED)
        xSemaphoreTake(flagMutex, portMAX_DELAY);
        currentTrafficState = TRAFFIC_YELLOW;
        trafficCountdown = 2;
        xSemaphoreGive(flagMutex);
        turn_off_all_leds();
        gpio_set_level(LED_YELLOW_GPIO, 1);
        for (int i = 0; i < 2; i++) {
            xSemaphoreTake(flagMutex, portMAX_DELAY);
            if (isEmergency || isNightMode) {
                trafficCountdown = 0;
                xSemaphoreGive(flagMutex);
                turn_off_all_leds();
                break;
            }
            trafficCountdown--;
            xSemaphoreGive(flagMutex);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

// ===== Task: Emergency =====
void EmergencyTask(void *pvParameters) {
    EventType_t event;
    bool ledOn = false;

    while (1) {
        if (xQueueReceive(eventQueueEmergency, &event, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (event == EMERGENCY_ON) {
                xSemaphoreTake(flagMutex, portMAX_DELAY);
                isEmergency = true;
                emergencyCountdown = 15;
                xSemaphoreGive(flagMutex);
                ESP_LOGI(TAG, "Emergency mode activated");
            } else if (event == EMERGENCY_OFF) {
                xSemaphoreTake(flagMutex, portMAX_DELAY);
                isEmergency = false;
                emergencyCountdown = 0;
                xSemaphoreGive(flagMutex);
                ESP_LOGI(TAG, "Emergency mode deactivated");
            }
        }

        xSemaphoreTake(flagMutex, portMAX_DELAY);
        bool checkEmergency = isEmergency;
        xSemaphoreGive(flagMutex);

        if (checkEmergency) {
            ledOn = !ledOn;
            turn_off_all_leds();
            gpio_set_level(LED_RED_GPIO, ledOn ? 1 : 0);

            xSemaphoreTake(flagMutex, portMAX_DELAY);
            if (isEmergency) {
                emergencyCountdown--;
                if (emergencyCountdown <= 0) {
                    isEmergency = false;
                    emergencyCountdown = 0;
                    ESP_LOGI(TAG, "Emergency mode auto-deactivated");
                }
            }
            xSemaphoreGive(flagMutex);

            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// ===== Task: Night Mode =====
void NightModeTask(void *pvParameters) {
    EventType_t event;
    bool ledOn = false;

    while (1) {
        if (xQueueReceive(eventQueueNight, &event, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (event == NIGHT_ON) {
                xSemaphoreTake(flagMutex, portMAX_DELAY);
                isNightMode = true;
                nightCountdown = 20;
                xSemaphoreGive(flagMutex);
                ESP_LOGI(TAG, "Night mode activated");
            } else if (event == NIGHT_OFF) {
                xSemaphoreTake(flagMutex, portMAX_DELAY);
                isNightMode = false;
                nightCountdown = 0;
                xSemaphoreGive(flagMutex);
                ESP_LOGI(TAG, "Night mode deactivated");
            }
        }

        xSemaphoreTake(flagMutex, portMAX_DELAY);
        bool checkNight = isNightMode;
        xSemaphoreGive(flagMutex);

        if (checkNight) {
            ledOn = !ledOn;
            turn_off_all_leds();
            gpio_set_level(LED_YELLOW_GPIO, ledOn ? 1 : 0);

            xSemaphoreTake(flagMutex, portMAX_DELAY);
            if (isNightMode) {
                nightCountdown--;
                if (nightCountdown <= 0) {
                    isNightMode = false;
                    nightCountdown = 0;
                    ESP_LOGI(TAG, "Night mode auto-deactivated");
                }
            }
            xSemaphoreGive(flagMutex);

            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// ===== Task: Monitor =====
void MonitorTask(void *pvParameters) {
    while (1) {
        xSemaphoreTake(flagMutex, portMAX_DELAY);
        bool checkEmergency = isEmergency;
        bool checkNight = isNightMode;
        int emergCount = emergencyCountdown;
        int nightCount = nightCountdown;
        int trafficCount = trafficCountdown;
        TrafficState_t trafficState = currentTrafficState;
        xSemaphoreGive(flagMutex);

        const char *trafficStateStr = (trafficState == TRAFFIC_RED) ? "🔴 Red" :
                                      (trafficState == TRAFFIC_YELLOW) ? "🟡 Yellow" : "🟢 Green";

        ESP_LOGI(TAG, "Traffic: %s (%d s) | 🚨 Emergency: %d (%d s) | 🌃 Night: %d (%d s)",
                 trafficStateStr, trafficCount,
                 checkEmergency, emergCount,
                 checkNight, nightCount);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ===== MAIN =====
void app_main(void) {
    ESP_LOGI(TAG, "=========================================================");
    ESP_LOGI(TAG, "========== Starting Smart Traffic Light System ==========");
    ESP_LOGI(TAG, "=========================================================");
    init_gpio();

    eventQueueEmergency = xQueueCreate(10, sizeof(EventType_t));
    eventQueueNight = xQueueCreate(10, sizeof(EventType_t));
    flagMutex = xSemaphoreCreateMutex();

    if (eventQueueEmergency == NULL || eventQueueNight == NULL || flagMutex == NULL) {
        ESP_LOGE(TAG, "Failed to create queue or mutex");
        return;
    }

    xTaskCreate(EmergencyTask, "EmergencyTask", 2048, NULL, 5, NULL);
    xTaskCreate(NightModeTask, "NightModeTask", 2048, NULL, 4, NULL);
    xTaskCreate(ButtonTask, "ButtonTask", 2048, NULL, 3, NULL);
    xTaskCreate(TrafficLightTask, "TrafficLightTask", 2048, NULL, 2, NULL);
    xTaskCreate(MonitorTask, "MonitorTask", 2048, NULL, 1, NULL);
}