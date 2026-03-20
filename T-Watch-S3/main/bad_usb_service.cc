#include "bad_usb_service.h"
#include <cstring>
#include <string_view>
#include "application.h"
#include "esp_log.h"
static const char* TAG = "BadUsbService";
BadUsbService::BadUsbService() : task_handle_(nullptr), is_service_running_(false) {
    hid_keyboard_ = std::make_unique<USBHIDKeyboard>();
    ducky_parser_ = std::make_unique<DuckyParser>(hid_keyboard_.get());
    command_queue_ = xQueueCreate(5, sizeof(BadUsbMessage));
}
BadUsbService::~BadUsbService() {
    if (task_handle_ != nullptr) {
        vTaskDelete(task_handle_);
    }
    if (command_queue_ != nullptr) {
        vQueueDelete(command_queue_);
    }
}
void BadUsbService::Start() {
    if (is_service_running_)
        return;
    ESP_LOGI(TAG, "Starting BadUsbService Task...");
    xTaskCreatePinnedToCore(UsbTaskWrapper, "BadUsbTask", 4096, this,
                            5,
                            &task_handle_,
                            0
    );
    is_service_running_ = true;
    ESP_LOGI(TAG, "BadUsbService Task Started on Core 0");
}
void BadUsbService::UsbTaskWrapper(void* arg) {
    BadUsbService* service = static_cast<BadUsbService*>(arg);
    service->UsbTask();
}
void BadUsbService::UsbTask() {
    BadUsbMessage msg;
    while (true) {
        if (xQueueReceive(command_queue_, &msg, portMAX_DELAY) == pdPASS) {

            if (msg.type == BadUsbCommandType::STOP) {
                ducky_parser_->stop();
                continue;
            }

            EnterCombatMode();

            hid_keyboard_->begin();

            vTaskDelay(pdMS_TO_TICKS(1500));

            hid_keyboard_->setLayoutByName(msg.lang);
            switch (msg.type) {
                case BadUsbCommandType::RUN_SCRIPT:
                    ESP_LOGI(TAG, "Executing DuckyScript (Lang: %s)...", msg.lang);
                    ducky_parser_->runScript(msg.payload);
                    break;
                case BadUsbCommandType::TYPE_TEXT:
                    ESP_LOGI(TAG, "Typing text directly (Lang: %s)...", msg.lang);
                    ducky_parser_->typeText(msg.payload);
                    break;
                case BadUsbCommandType::STOP:
                    ESP_LOGI(TAG, "Stopping BadUSB execution.");
                    ducky_parser_->stop();
                    break;
            }

            hid_keyboard_->end();

            ExitCombatMode();
        }
    }
}
void BadUsbService::EnterCombatMode() {
    auto& app = Application::GetInstance();
    app.GetAudioService().Stop();
    if (app.GetRadioService().IsJamming()) {
        app.GetRadioService().StopJammer();
    }
    ESP_LOGW(TAG,
             "SAVAS MODU AKTIF! Agirliklar atildi (Ses, RF Jammer), donanim hizlandiriliyor.");
    ESP_LOGI(TAG, "Wi-Fi ve Mikrofon acik tutuluyor, yapay zeka dinlemede kalacak.");
}
void BadUsbService::ExitCombatMode() {
    auto& app = Application::GetInstance();
    app.GetAudioService().Start();
    ESP_LOGI(TAG, "SAVAS MODU KAPALI. Normale donuldu.");
}
bool BadUsbService::RunScript(const std::string& script, const std::string& lang) {
    if (!is_service_running_)
        Start();
    BadUsbMessage msg;
    msg.type = BadUsbCommandType::RUN_SCRIPT;
    size_t copy_len = std::min(script.length(), sizeof(msg.payload) - 1);
    std::memcpy(msg.payload, script.c_str(), copy_len);
    msg.payload[copy_len] = '\0';
    strncpy(msg.lang, lang.c_str(), sizeof(msg.lang) - 1);
    msg.lang[sizeof(msg.lang) - 1] = '\0';
    return xQueueSend(command_queue_, &msg, 0) == pdPASS;
}
bool BadUsbService::TypeText(const std::string& text, const std::string& lang) {
    if (!is_service_running_)
        Start();
    BadUsbMessage msg;
    msg.type = BadUsbCommandType::TYPE_TEXT;
    size_t copy_len = std::min(text.length(), sizeof(msg.payload) - 1);
    std::memcpy(msg.payload, text.c_str(), copy_len);
    msg.payload[copy_len] = '\0';
    strncpy(msg.lang, lang.c_str(), sizeof(msg.lang) - 1);
    msg.lang[sizeof(msg.lang) - 1] = '\0';
    return xQueueSend(command_queue_, &msg, 0) == pdPASS;
}
void BadUsbService::Stop() {
    if (!is_service_running_)
        return;
    ducky_parser_->stop();
    BadUsbMessage msg;
    msg.type = BadUsbCommandType::STOP;
    xQueueSendToFront(command_queue_, &msg, 0);
}
std::string BadUsbService::GetStatusJSON() {
    bool hid_connected = hid_keyboard_->isConnected();
    bool writing = ducky_parser_->isRunning();

    bool physically_connected = hid_connected;
    if (!physically_connected) {
        int level;
        bool charging, discharging;

        if (Board::GetInstance().GetBatteryLevel(level, charging, discharging)) {
            physically_connected = charging;
        }
    }

    char json[128];
    snprintf(json, sizeof(json), "{\"usb_connected\": %s, \"is_typing\": %s}",
             physically_connected ? "true" : "false", writing ? "true" : "false");
    return std::string(json);
}
bool BadUsbService::IsRunning() const { return ducky_parser_->isRunning(); }