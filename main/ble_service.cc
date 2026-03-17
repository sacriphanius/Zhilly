#include "ble_service.h"
#include <cstring>
#include <string_view>
#include "application.h"
#include "esp_log.h"
#include "bad_usb/BleKeyboard.h"

#if defined(CONFIG_BT_ENABLED)

static const char* TAG = "BleService";

enum class BleCommandType {
    RUN_SCRIPT,
    TYPE_TEXT,
    STOP
};

struct BleMessage {
    BleCommandType type;
    char payload[1024]; 
};

BleService::BleService() : 
    ble_keyboard(nullptr), 
    ducky_parser(nullptr), 
    task_handle(nullptr), 
    command_queue(nullptr), 
    is_running(false), 
    ble_active(false) {
    
    // We create the parser only when BLE is active to save RAM or just keep it around.
    // Given the lightweight nature of DuckyParser, we can instantiate it on demand, 
    // but command queue must exist if we want to receive commands before start.
    command_queue = xQueueCreate(5, sizeof(BleMessage));
}

BleService::~BleService() {
    stop();
    if (command_queue != nullptr) {
        vQueueDelete(command_queue);
    }
}

bool BleService::start() {
    if (ble_active) {
        ESP_LOGI(TAG, "BLE already active.");
        return true;
    }

    ESP_LOGI(TAG, "Starting BLE Service...");

    if (!ble_keyboard) {
        ble_keyboard = new BleKeyboard("Zhilly BLE V2", "Espressif", 100);
        ducky_parser = new DuckyParser(ble_keyboard);
    }

    ble_keyboard->begin();

    if (!is_running) {
        xTaskCreatePinnedToCore(taskLoop, "BleWorker", 4096, this, 5, &task_handle, 0);
        is_running = true;
    }
    
    ble_active = true;
    ESP_LOGI(TAG, "BleService started and advertising.");
    return true;
}

void BleService::stop() {
    if (!ble_active) {
        return;
    }
    
    ESP_LOGI(TAG, "Stopping BLE Service & Freeing RAM.");
    
    if (ducky_parser) {
        ducky_parser->stop();
    }

    if (ble_keyboard) {
        ble_keyboard->end();
        delete ducky_parser;
        ducky_parser = nullptr;
        
        delete ble_keyboard;
        ble_keyboard = nullptr;
    }

    if (is_running) {
        BleMessage msg;
        msg.type = BleCommandType::STOP;
        xQueueSendToFront(command_queue, &msg, 0); 
    }

    ble_active = false;
    ESP_LOGI(TAG, "BleService stopped completely.");
}

void BleService::taskLoop(void* arg) {
    BleService* service = static_cast<BleService*>(arg);
    BleMessage msg;

    while (true) {
        if (xQueueReceive(service->command_queue, &msg, portMAX_DELAY) == pdPASS) {
            
            if (!service->ble_active && msg.type != BleCommandType::STOP) {
                ESP_LOGW(TAG, "Command received but BLE is closed. Auto-starting...");
                service->start();
            }

            if (msg.type == BleCommandType::STOP) {
                if(service->ducky_parser) service->ducky_parser->stop();
                break; // Exit the task loop when completely stopped if we want task to die
            }

            // Simple combat mode concept for logging
            ESP_LOGI(TAG, "Pausing audio for BLE processing...");
            auto& app = Application::GetInstance();
            app.GetAudioService().Stop();

            if (!service->isConnected()) {
                ESP_LOGW(TAG, "BLE Keyboard not connected to any device. Waiting up to 10 seconds...");
                for(int i = 0; i < 100; i++) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                    if (service->isConnected()) break;
                }
            }

            if (!service->isConnected()) {
                ESP_LOGE(TAG, "Failed to execute: BLE device not connected.");
                app.GetAudioService().Start();
                continue;
            }

            switch (msg.type) {
                case BleCommandType::RUN_SCRIPT:
                    ESP_LOGI(TAG, "Executing BLE DuckyScript...");
                    if(service->ducky_parser) service->ducky_parser->runScript(msg.payload);
                    break;
                case BleCommandType::TYPE_TEXT:
                    ESP_LOGI(TAG, "Typing text over BLE...");
                    if(service->ducky_parser) service->ducky_parser->typeText(msg.payload);
                    break;
                default:
                    break;
            }

            ESP_LOGI(TAG, "BLE execution done. Resuming audio.");
            app.GetAudioService().Start();
        }
    }
    
    service->is_running = false;
    service->task_handle = nullptr;
    vTaskDelete(NULL);
}

bool BleService::execute(const std::string& script) {
    BleMessage msg;
    msg.type = BleCommandType::RUN_SCRIPT;
    size_t copy_len = std::min(script.length(), sizeof(msg.payload) - 1);
    std::memcpy(msg.payload, script.c_str(), copy_len);
    msg.payload[copy_len] = '\0';
    return xQueueSend(command_queue, &msg, 0) == pdPASS;
}

bool BleService::typeText(const std::string& text) {
    BleMessage msg;
    msg.type = BleCommandType::TYPE_TEXT;
    size_t copy_len = std::min(text.length(), sizeof(msg.payload) - 1);
    std::memcpy(msg.payload, text.c_str(), copy_len);
    msg.payload[copy_len] = '\0';
    return xQueueSend(command_queue, &msg, 0) == pdPASS;
}

void BleService::stopScript() {
    if (ducky_parser) {
        ducky_parser->stop();
    }
}

bool BleService::isConnected() const {
    if (ble_keyboard) return ble_keyboard->isConnected();
    return false;
}

size_t BleService::getQueueSize() const {
    if (command_queue) return uxQueueMessagesWaiting(command_queue);
    return 0;
}

#endif
