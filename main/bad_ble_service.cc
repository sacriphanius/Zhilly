#include "bad_ble_service.h"
#include "esp_log.h"

// Resolve conflict between esp_hid and TinyUSB
#ifdef HID_USAGE_CONSUMER_CONTROL
#undef HID_USAGE_CONSUMER_CONTROL
#endif

#include "application.h"

static const char* TAG = "BadBleService";

BadBleService::BadBleService() 
    : ble_keyboard_(nullptr), 
      ducky_parser_(nullptr), 
      task_handle_(nullptr), 
      command_queue_(nullptr), 
      is_service_running_(false) {
}

BadBleService::~BadBleService() {
    Stop();
}

void BadBleService::Start() {
    if (is_service_running_) return;

    command_queue_ = xQueueCreate(10, sizeof(BadBleMessage));
    ble_keyboard_ = std::make_unique<BleKeyboard>();
    ducky_parser_ = std::make_unique<DuckyParser>(ble_keyboard_.get());

    ble_keyboard_->begin();

    xTaskCreate(BleTaskWrapper, "bad_ble_task", 8192, this, 5, &task_handle_);
    is_service_running_ = true;
    ESP_LOGI(TAG, "BadBleService started");
}

void BadBleService::Stop() {
    if (!is_service_running_) return;

    BadBleMessage msg = {BadBleCommandType::STOP, {0}, {0}};
    xQueueSend(command_queue_, &msg, portMAX_DELAY);

    if (task_handle_) {
        // Wait for task to finish or delete it
        // For simplicity, we just mark it as stopped and let it exit
    }

    if (command_queue_) {
        vQueueDelete(command_queue_);
        command_queue_ = nullptr;
    }

    ble_keyboard_->end();
    is_service_running_ = false;
    ESP_LOGI(TAG, "BadBleService stopped");
}

bool BadBleService::RunScript(const std::string& script, const std::string& lang) {
    if (!is_service_running_) return false;

    BadBleMessage msg;
    msg.type = BadBleCommandType::RUN_SCRIPT;
    strncpy(msg.payload, script.c_str(), sizeof(msg.payload) - 1);
    strncpy(msg.lang, lang.c_str(), sizeof(msg.lang) - 1);
    
    return xQueueSend(command_queue_, &msg, 0) == pdTRUE;
}

bool BadBleService::TypeText(const std::string& text, const std::string& lang) {
    if (!is_service_running_) return false;

    BadBleMessage msg;
    msg.type = BadBleCommandType::TYPE_TEXT;
    strncpy(msg.payload, text.c_str(), sizeof(msg.payload) - 1);
    strncpy(msg.lang, lang.c_str(), sizeof(msg.lang) - 1);

    return xQueueSend(command_queue_, &msg, 0) == pdTRUE;
}

void BadBleService::BleTaskWrapper(void* arg) {
    static_cast<BadBleService*>(arg)->BleTask();
}

void BadBleService::BleTask() {
    BadBleMessage msg;
    while (xQueueReceive(command_queue_, &msg, portMAX_DELAY)) {
        if (msg.type == BadBleCommandType::STOP) break;

        // Set layout
        ble_keyboard_->setLayoutByName(msg.lang);

        EnterCombatMode();
        if (msg.type == BadBleCommandType::RUN_SCRIPT) {
            ducky_parser_->runScript(msg.payload);
        } else if (msg.type == BadBleCommandType::TYPE_TEXT) {
            ducky_parser_->typeText(msg.payload);
        }
        ExitCombatMode();
    }
    task_handle_ = nullptr;
    vTaskDelete(NULL);
}

void BadBleService::SetDeviceName(const std::string& name) {
    if (ble_keyboard_) {
        ble_keyboard_->setDeviceName(name);
    }
}

bool BadBleService::IsRunning() const {
    return ducky_parser_ && ducky_parser_->isRunning();
}

bool BadBleService::IsConnected() const {
    return ble_keyboard_ && ble_keyboard_->isConnected();
}

void BadBleService::EnterCombatMode() {
    auto& app = Application::GetInstance();
    app.GetAudioService().Stop();
    app.GetIrService().StopTvBGone();
    app.GetIrService().StopJammer();
    app.GetCc1101Service().StopReplay();
    app.GetCc1101Service().StopJammer();
    ESP_LOGW(TAG, "SAVAS MODU AKTIF! (BLE) Agirliklar atildi (Ses, IR, CC1101)");
}

void BadBleService::ExitCombatMode() {
    // Optional: restart some services or just log
    ESP_LOGI(TAG, "SAVAS MODU TAMAMLANDI (BLE)");
}

std::string BadBleService::GetStatusJSON() {
    return "{\"running\":" + std::to_string(IsRunning()) + 
           ",\"connected\":" + std::to_string(IsConnected()) + "}";
}
