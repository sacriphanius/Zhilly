

#ifndef __OSCILLATOR_H__
#define __OSCILLATOR_H__

#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define M_PI 3.14159265358979323846

#ifndef DEG2RAD
#define DEG2RAD(g) ((g) * M_PI) / 180
#endif

#define SERVO_MIN_PULSEWIDTH_US 500           
#define SERVO_MAX_PULSEWIDTH_US 2500          
#define SERVO_MIN_DEGREE -90                  
#define SERVO_MAX_DEGREE 90                   
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  
#define SERVO_TIMEBASE_PERIOD 20000           

class Oscillator {
public:
    Oscillator(int trim = 0);
    ~Oscillator();
    void Attach(int pin, bool rev = false);
    void Detach();

    void SetA(unsigned int amplitude) { amplitude_ = amplitude; };
    void SetO(int offset) { offset_ = offset; };
    void SetPh(double Ph) { phase0_ = Ph; };
    void SetT(unsigned int period);
    void SetTrim(int trim) { trim_ = trim; };
    void SetLimiter(int diff_limit) { diff_limit_ = diff_limit; };
    void DisableLimiter() { diff_limit_ = 0; };
    int GetTrim() { return trim_; };
    void SetPosition(int position);
    void Stop() { stop_ = true; };
    void Play() { stop_ = false; };
    void Reset() { phase_ = 0; };
    void Refresh();
    int GetPosition() { return pos_; }

private:
    bool NextSample();
    void Write(int position);
    uint32_t AngleToCompare(int angle);

private:
    bool is_attached_;

    unsigned int amplitude_;  
    int offset_;              
    unsigned int period_;     
    double phase0_;           

    int pos_;                       
    int pin_;                       
    int trim_;                      
    double phase_;                  
    double inc_;                    
    double number_samples_;         
    unsigned int sampling_period_;  

    long previous_millis_;
    long current_millis_;

    bool stop_;

    bool rev_;

    int diff_limit_;
    long previous_servo_command_millis_;

    ledc_channel_t ledc_channel_;
    ledc_mode_t ledc_speed_mode_;
};

#endif  