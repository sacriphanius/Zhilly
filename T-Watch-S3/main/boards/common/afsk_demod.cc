#include "afsk_demod.h"
#include <cstring>
#include <algorithm>
#include "esp_log.h"
#include "display.h"
#include "ssid_manager.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace audio_wifi_config
{
    static const char *kLogTag = "AUDIO_WIFI_CONFIG";

    void ReceiveWifiCredentialsFromAudio(Application *app,
                                        WifiManager *wifi_manager,
                                        Display *display,
                                        size_t input_channels
                                    )
    {
        const int kInputSampleRate = 16000;

        const float kDownsampleStep = static_cast<float>(kInputSampleRate) / static_cast<float>(kAudioSampleRate);

        std::vector<int16_t> audio_data;
        AudioSignalProcessor signal_processor(kAudioSampleRate, kMarkFrequency, kSpaceFrequency, kBitRate, kWindowSize);
        AudioDataBuffer data_buffer;

        while (true)
        {

            if (app->GetDeviceState() != kDeviceStateWifiConfiguring) {

                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            if (!app->GetAudioService().ReadAudioData(audio_data, 16000, 480)) {

                ESP_LOGI(kLogTag, "Failed to read audio data, retrying.");
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }

            if (input_channels == 2) {

                auto mono_data = std::vector<int16_t>(audio_data.size() / 2);
                for (size_t i = 0, j = 0; i < mono_data.size(); ++i, j += 2) {
                    mono_data[i] = audio_data[j];
                }
                audio_data = std::move(mono_data);
            }

            std::vector<float> downsampled_data;
            size_t last_index = 0;

            if (kDownsampleStep > 1.0f) {
                downsampled_data.reserve(audio_data.size() / static_cast<size_t>(kDownsampleStep));
                for (size_t i = 0; i < audio_data.size(); ++i) {
                    size_t sample_index = static_cast<size_t>(i / kDownsampleStep);
                    if ((sample_index + 1) > last_index) {
                        downsampled_data.push_back(static_cast<float>(audio_data[i]));
                        last_index = sample_index + 1;
                    }
                }
            } else {
                downsampled_data.reserve(audio_data.size());
                for (int16_t sample : audio_data) {
                    downsampled_data.push_back(static_cast<float>(sample));
                }
            }

            auto probabilities = signal_processor.ProcessAudioSamples(downsampled_data);

            if (data_buffer.ProcessProbabilityData(probabilities, 0.5f)) {

                if (data_buffer.decoded_text.has_value()) {
                    ESP_LOGI(kLogTag, "Received text data: %s", data_buffer.decoded_text->c_str());
                    display->SetChatMessage("system", data_buffer.decoded_text->c_str());

                    std::string wifi_ssid, wifi_password;
                    size_t newline_position = data_buffer.decoded_text->find('\n');
                    if (newline_position != std::string::npos) {
                        wifi_ssid = data_buffer.decoded_text->substr(0, newline_position);
                        wifi_password = data_buffer.decoded_text->substr(newline_position + 1);
                        ESP_LOGI(kLogTag, "WiFi SSID: %s, Password: %s", wifi_ssid.c_str(), wifi_password.c_str());
                    } else {
                        ESP_LOGE(kLogTag, "Invalid data format, no newline character found");
                        continue;
                    }

                    auto& ssid_manager = SsidManager::GetInstance();
                    ssid_manager.AddSsid(wifi_ssid, wifi_password);
                    ESP_LOGI(kLogTag, "WiFi credentials saved successfully");

                    wifi_manager->StopConfigAp();

                    data_buffer.decoded_text.reset();

                    return;

                }
            }
            vTaskDelay(pdMS_TO_TICKS(1));

        }
    }

    const std::vector<uint8_t> kDefaultStartTransmissionPattern = {
        0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0};

    const std::vector<uint8_t> kDefaultEndTransmissionPattern = {
        0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0};

    FrequencyDetector::FrequencyDetector(float frequency, size_t window_size)
        : frequency_(frequency), window_size_(window_size) {
        frequency_bin_ = std::floor(frequency_ * static_cast<float>(window_size_));
        angular_frequency_ = 2.0f * M_PI * frequency_;
        cos_coefficient_ = std::cos(angular_frequency_);
        sin_coefficient_ = std::sin(angular_frequency_);
        filter_coefficient_ = 2.0f * cos_coefficient_;

        state_buffer_.push_back(0.0f);
        state_buffer_.push_back(0.0f);
    }

    void FrequencyDetector::Reset() {
        state_buffer_.clear();
        state_buffer_.push_back(0.0f);
        state_buffer_.push_back(0.0f);
    }

    void FrequencyDetector::ProcessSample(float sample) {
        if (state_buffer_.size() < 2) {
            return;
        }

        float s_minus_2 = state_buffer_.front();

        state_buffer_.pop_front();
        float s_minus_1 = state_buffer_.front();

        state_buffer_.pop_front();

        float s_current = sample + filter_coefficient_ * s_minus_1 - s_minus_2;

        state_buffer_.push_back(s_minus_1);

        state_buffer_.push_back(s_current);

    }

    float FrequencyDetector::GetAmplitude() const {
        if (state_buffer_.size() < 2) {
            return 0.0f;
        }

        float s_minus_1 = state_buffer_[1];

        float s_minus_2 = state_buffer_[0];

        float real_part = cos_coefficient_ * s_minus_1 - s_minus_2;

        float imaginary_part = sin_coefficient_ * s_minus_1;

        return std::sqrt(real_part * real_part + imaginary_part * imaginary_part) /
               (static_cast<float>(window_size_) / 2.0f);
    }

    AudioSignalProcessor::AudioSignalProcessor(size_t sample_rate, size_t mark_frequency, size_t space_frequency,
                                             size_t bit_rate, size_t window_size)
        : input_buffer_size_(window_size), output_sample_count_(0) {
        if (sample_rate % bit_rate != 0) {

            ESP_LOGW(kLogTag, "Sample rate %zu is not divisible by bit rate %zu", sample_rate, bit_rate);
        }

        float normalized_mark_freq = static_cast<float>(mark_frequency) / static_cast<float>(sample_rate);
        float normalized_space_freq = static_cast<float>(space_frequency) / static_cast<float>(sample_rate);

        mark_detector_ = std::make_unique<FrequencyDetector>(normalized_mark_freq, window_size);
        space_detector_ = std::make_unique<FrequencyDetector>(normalized_space_freq, window_size);

        samples_per_bit_ = sample_rate / bit_rate;

    }

    std::vector<float> AudioSignalProcessor::ProcessAudioSamples(const std::vector<float> &samples) {
        std::vector<float> result;

        for (float sample : samples) {
            if (input_buffer_.size() < input_buffer_size_) {
                input_buffer_.push_back(sample);

            } else {

                input_buffer_.pop_front();

                input_buffer_.push_back(sample);

                output_sample_count_++;

                if (output_sample_count_ >= samples_per_bit_) {

                    for (float window_sample : input_buffer_) {
                        mark_detector_->ProcessSample(window_sample);
                        space_detector_->ProcessSample(window_sample);
                    }

                    float mark_amplitude = mark_detector_->GetAmplitude();

                    float space_amplitude = space_detector_->GetAmplitude();

                    float mark_probability = mark_amplitude /
                                           (space_amplitude + mark_amplitude + std::numeric_limits<float>::epsilon());
                    result.push_back(mark_probability);

                    mark_detector_->Reset();
                    space_detector_->Reset();
                    output_sample_count_ = 0;

                }
            }
        }

        return result;
    }

    AudioDataBuffer::AudioDataBuffer()
        : current_state_(DataReceptionState::kInactive),
          start_of_transmission_(kDefaultStartTransmissionPattern),
          end_of_transmission_(kDefaultEndTransmissionPattern),
          enable_checksum_validation_(true) {
        identifier_buffer_size_ = std::max(start_of_transmission_.size(), end_of_transmission_.size());
        max_bit_buffer_size_ = 776;

        bit_buffer_.reserve(max_bit_buffer_size_);
    }

    AudioDataBuffer::AudioDataBuffer(size_t max_byte_size, const std::vector<uint8_t> &start_identifier,
                                   const std::vector<uint8_t> &end_identifier, bool enable_checksum)
        : current_state_(DataReceptionState::kInactive),
          start_of_transmission_(start_identifier),
          end_of_transmission_(end_identifier),
          enable_checksum_validation_(enable_checksum) {
        identifier_buffer_size_ = std::max(start_of_transmission_.size(), end_of_transmission_.size());
        max_bit_buffer_size_ = max_byte_size * 8;

        bit_buffer_.reserve(max_bit_buffer_size_);
    }

    uint8_t AudioDataBuffer::CalculateChecksum(const std::string &text) {
        uint8_t checksum = 0;
        for (char character : text) {
            checksum += static_cast<uint8_t>(character);
        }
        return checksum;
    }

    void AudioDataBuffer::ClearBuffers() {
        identifier_buffer_.clear();
        bit_buffer_.clear();
    }

    bool AudioDataBuffer::ProcessProbabilityData(const std::vector<float> &probabilities, float threshold) {
        for (float probability : probabilities) {
            uint8_t bit = (probability > threshold) ? 1 : 0;

            if (identifier_buffer_.size() >= identifier_buffer_size_) {
                identifier_buffer_.pop_front();

            }
            identifier_buffer_.push_back(bit);

            switch (current_state_) {
            case DataReceptionState::kInactive:
                if (identifier_buffer_.size() >= start_of_transmission_.size()) {
                    current_state_ = DataReceptionState::kWaiting;

                    ESP_LOGI(kLogTag, "Entering Waiting state");
                }
                break;

            case DataReceptionState::kWaiting:

                if (identifier_buffer_.size() >= start_of_transmission_.size()) {
                    std::vector<uint8_t> identifier_snapshot(identifier_buffer_.begin(), identifier_buffer_.end());
                    if (identifier_snapshot == start_of_transmission_)
                    {
                        ClearBuffers();

                        current_state_ = DataReceptionState::kReceiving;

                        ESP_LOGI(kLogTag, "Entering Receiving state");
                    }
                }
                break;

            case DataReceptionState::kReceiving:
                bit_buffer_.push_back(bit);
                if (identifier_buffer_.size() >= end_of_transmission_.size()) {
                    std::vector<uint8_t> identifier_snapshot(identifier_buffer_.begin(), identifier_buffer_.end());
                    if (identifier_snapshot == end_of_transmission_) {
                        current_state_ = DataReceptionState::kInactive;

                        std::vector<uint8_t> bytes = ConvertBitsToBytes(bit_buffer_);

                        uint8_t received_checksum = 0;
                        size_t minimum_length = 0;

                        if (enable_checksum_validation_) {

                            minimum_length = 1 + start_of_transmission_.size() / 8;
                            if (bytes.size() >= minimum_length)
                            {
                                received_checksum = bytes[bytes.size() - start_of_transmission_.size() / 8 - 1];
                            }
                        } else {
                            minimum_length = start_of_transmission_.size() / 8;
                        }

                        if (bytes.size() < minimum_length) {
                            ClearBuffers();
                            ESP_LOGW(kLogTag, "Data too short, clearing buffer");
                            return false;

                        }

                        std::vector<uint8_t> text_bytes(
                            bytes.begin(), bytes.begin() + bytes.size() - minimum_length);

                        std::string result(text_bytes.begin(), text_bytes.end());

                        if (enable_checksum_validation_) {
                            uint8_t calculated_checksum = CalculateChecksum(result);
                            if (calculated_checksum != received_checksum) {

                                ESP_LOGW(kLogTag, "Checksum mismatch: expected %d, got %d",
                                        received_checksum, calculated_checksum);
                                ClearBuffers();
                                return false;
                            }
                        }

                        ClearBuffers();
                        decoded_text = result;
                        return true;

                    } else if (bit_buffer_.size() >= max_bit_buffer_size_) {

                        ClearBuffers();
                        ESP_LOGW(kLogTag, "Buffer overflow, clearing buffer");
                        current_state_ = DataReceptionState::kInactive;

                    }
                }
                break;
            }
        }

        return false;
    }

    std::vector<uint8_t> AudioDataBuffer::ConvertBitsToBytes(const std::vector<uint8_t> &bits) const {
        std::vector<uint8_t> bytes;

        size_t complete_bytes_count = bits.size() / 8;
        bytes.reserve(complete_bytes_count);

        for (size_t i = 0; i < complete_bytes_count; ++i) {
            uint8_t byte_value = 0;
            for (size_t j = 0; j < 8; ++j) {
                byte_value |= bits[i * 8 + j] << (7 - j);
            }
            bytes.push_back(byte_value);
        }

        return bytes;
    }
}