#ifndef __EDA_DOG_MOVEMENTS_H__
#define __EDA_DOG_MOVEMENTS_H__

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "oscillator.h"

#define FORWARD 1
#define BACKWARD -1
#define LEFT 1
#define RIGHT -1
#define SMALL 5
#define MEDIUM 15
#define BIG 30

#define SERVO_LIMIT_DEFAULT 240

#define LEFT_FRONT_LEG 0
#define LEFT_REAR_LEG 1
#define RIGHT_FRONT_LEG 2
#define RIGHT_REAR_LEG 3
#define SERVO_COUNT 4

class EDARobotDog {
public:
    EDARobotDog();
    ~EDARobotDog();

    void Init(int left_front_leg, int left_rear_leg, int right_front_leg, int right_rear_leg);

    void AttachServos();
    void DetachServos();

    void SetTrims(int left_front_leg, int left_rear_leg, int right_front_leg, int right_rear_leg);

    void MoveServos(int time, int servo_target[]);
    void MoveSingle(int position, int servo_number);
    void OscillateServos(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
                         double phase_diff[SERVO_COUNT], float cycle);

    void Home();
    bool GetRestState();
    void SetRestState(bool state);

    void LiftLeftFrontLeg(int period = 1000, int height = 45);   
    void LiftLeftRearLeg(int period = 1000, int height = 45);    
    void LiftRightFrontLeg(int period = 1000, int height = 45);  
    void LiftRightRearLeg(int period = 1000, int height = 45);   

    void Walk(float steps = 4, int period = 1000, int dir = FORWARD);
    void Turn(float steps = 4, int period = 2000, int dir = LEFT);
    void Sit(int period = 1500);
    void Stand(int period = 1500);
    void Stretch(int period = 2000);
    void Shake(int period = 1000);
    void Sleep();  

    void EnableServoLimit(int speed_limit_degree_per_sec = SERVO_LIMIT_DEFAULT);
    void DisableServoLimit();

private:
    Oscillator servo_[SERVO_COUNT];

    int servo_pins_[SERVO_COUNT];
    int servo_trim_[SERVO_COUNT];

    unsigned long final_time_;
    unsigned long partial_time_;
    float increment_[SERVO_COUNT];

    bool is_dog_resting_;

    void Execute(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
                 double phase_diff[SERVO_COUNT], float steps);
};

#endif  