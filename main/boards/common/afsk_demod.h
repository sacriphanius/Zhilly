#pragma once

#include <vector>
#include <deque>
#include <string>
#include <memory>
#include <optional>
#include <cmath>
#include "wifi_manager.h"
#include "application.h"

const size_t kAudioSampleRate = 6400;
const size_t kMarkFrequency = 1800;
const size_t kSpaceFrequency = 1500;
const size_t kBitRate = 100;
const size_t kWindowSize = 64;

namespace audio_wifi_config
{

    void ReceiveWifiCredentialsFromAudio(Application *app, WifiManager *wifi_manager, Display *display, 
                                         size_t input_channels = 1);

    class FrequencyDetector
    {
    private:
        float frequency_;              
        size_t window_size_;           
        float frequency_bin_;          
        float angular_frequency_;      
        float cos_coefficient_;        
        float sin_coefficient_;        
        float filter_coefficient_;     
        std::deque<float> state_buffer_;  

    public:

        FrequencyDetector(float frequency, size_t window_size);

        void Reset();

        void ProcessSample(float sample);

        float GetAmplitude() const;
    };

    class AudioSignalProcessor
    {
    private:
        std::deque<float> input_buffer_;             
        size_t input_buffer_size_;                   
        size_t output_sample_count_;                 
        size_t samples_per_bit_;                     
        std::unique_ptr<FrequencyDetector> mark_detector_;   
        std::unique_ptr<FrequencyDetector> space_detector_;  

    public:

        AudioSignalProcessor(size_t sample_rate, size_t mark_frequency, size_t space_frequency,
                           size_t bit_rate, size_t window_size);

        std::vector<float> ProcessAudioSamples(const std::vector<float> &samples);
    };

    enum class DataReceptionState
    {
        kInactive,  
        kWaiting,   
        kReceiving  
    };

    class AudioDataBuffer
    {
    private:
        DataReceptionState current_state_;       
        std::deque<uint8_t> identifier_buffer_;  
        size_t identifier_buffer_size_;          
        std::vector<uint8_t> bit_buffer_;        
        size_t max_bit_buffer_size_;             
        const std::vector<uint8_t> start_of_transmission_;  
        const std::vector<uint8_t> end_of_transmission_;    
        bool enable_checksum_validation_;       

    public:
        std::optional<std::string> decoded_text; 

        AudioDataBuffer();

        AudioDataBuffer(size_t max_byte_size, const std::vector<uint8_t> &start_identifier,
                      const std::vector<uint8_t> &end_identifier, bool enable_checksum = false);

        bool ProcessProbabilityData(const std::vector<float> &probabilities, float threshold = 0.5f);

        static uint8_t CalculateChecksum(const std::string &text);

    private:

        std::vector<uint8_t> ConvertBitsToBytes(const std::vector<uint8_t> &bits) const;

        void ClearBuffers();
    };

    extern const std::vector<uint8_t> kDefaultStartTransmissionPattern;
    extern const std::vector<uint8_t> kDefaultEndTransmissionPattern;
}