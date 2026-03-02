#include "eda_super_bear_movements.h"

#include <algorithm>

#include "oscillator.h"

static const char* TAG = "EdaSuperBearMovements";

#define HAND_HOME_POSITION 45

EdaRobot::EdaRobot() {
    is_edarobot_resting_ = false;
    has_hands_ = false;

    for (int i = 0; i < SERVO_COUNT; i++) {
        servo_pins_[i] = -1;
        servo_trim_[i] = 0;
    }
}

EdaRobot::~EdaRobot() {
    DetachServos();
}

unsigned long IRAM_ATTR millis() {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

void EdaRobot::Init(int left_leg, int right_leg, int left_foot, int right_foot, int left_hand,
                int right_hand) {
    servo_pins_[LEFT_LEG] = left_leg;
    servo_pins_[RIGHT_LEG] = right_leg;
    servo_pins_[LEFT_FOOT] = left_foot;
    servo_pins_[RIGHT_FOOT] = right_foot;
    servo_pins_[LEFT_HAND] = left_hand;
    servo_pins_[RIGHT_HAND] = right_hand;

    has_hands_ = (left_hand != -1 && right_hand != -1);

    AttachServos();
    is_edarobot_resting_ = false;
}

void EdaRobot::AttachServos() {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].Attach(servo_pins_[i]);
        }
    }
}

void EdaRobot::DetachServos() {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].Detach();
        }
    }
}

void EdaRobot::SetTrims(int left_leg, int right_leg, int left_foot, int right_foot, int left_hand,
                    int right_hand) {
    servo_trim_[LEFT_LEG] = left_leg;
    servo_trim_[RIGHT_LEG] = right_leg;
    servo_trim_[LEFT_FOOT] = left_foot;
    servo_trim_[RIGHT_FOOT] = right_foot;

    if (has_hands_) {
        servo_trim_[LEFT_HAND] = left_hand;
        servo_trim_[RIGHT_HAND] = right_hand;
    }

    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].SetTrim(servo_trim_[i]);
        }
    }
}

void EdaRobot::MoveServos(int time, int servo_target[]) {
    if (GetRestState() == true) {
        SetRestState(false);
    }

    final_time_ = millis() + time;
    if (time > 10) {
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (servo_pins_[i] != -1) {
                increment_[i] = (servo_target[i] - servo_[i].GetPosition()) / (time / 10.0);
            }
        }

        for (int iteration = 1; millis() < final_time_; iteration++) {
            partial_time_ = millis() + 10;
            for (int i = 0; i < SERVO_COUNT; i++) {
                if (servo_pins_[i] != -1) {
                    servo_[i].SetPosition(servo_[i].GetPosition() + increment_[i]);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    } else {
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (servo_pins_[i] != -1) {
                servo_[i].SetPosition(servo_target[i]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(time));
    }

    bool f = true;
    int adjustment_count = 0;
    while (f && adjustment_count < 10) {
        f = false;
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (servo_pins_[i] != -1 && servo_target[i] != servo_[i].GetPosition()) {
                f = true;
                break;
            }
        }
        if (f) {
            for (int i = 0; i < SERVO_COUNT; i++) {
                if (servo_pins_[i] != -1) {
                    servo_[i].SetPosition(servo_target[i]);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            adjustment_count++;
        }
    };
}

void EdaRobot::MoveSingle(int position, int servo_number) {
    if (position > 180)
        position = 90;
    if (position < 0)
        position = 90;

    if (GetRestState() == true) {
        SetRestState(false);
    }

    if (servo_number >= 0 && servo_number < SERVO_COUNT && servo_pins_[servo_number] != -1) {
        servo_[servo_number].SetPosition(position);
    }
}

void EdaRobot::OscillateServos(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
                           double phase_diff[SERVO_COUNT], float cycle = 1) {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].SetO(offset[i]);
            servo_[i].SetA(amplitude[i]);
            servo_[i].SetT(period);
            servo_[i].SetPh(phase_diff[i]);
        }
    }

    double ref = millis();
    double end_time = period * cycle + ref;

    while (millis() < end_time) {
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (servo_pins_[i] != -1) {
                servo_[i].Refresh();
            }
        }
        vTaskDelay(5);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}

void EdaRobot::Execute(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
                   double phase_diff[SERVO_COUNT], float steps = 1.0) {
    if (GetRestState() == true) {
        SetRestState(false);
    }

    int cycles = (int)steps;

    if (cycles >= 1)
        for (int i = 0; i < cycles; i++)
            OscillateServos(amplitude, offset, period, phase_diff);

    OscillateServos(amplitude, offset, period, phase_diff, (float)steps - cycles);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void EdaRobot::Home(bool hands_down) {
    if (is_edarobot_resting_ == false) {  

        int homes[SERVO_COUNT];
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (i == LEFT_HAND || i == RIGHT_HAND) {
                if (hands_down) {

                    if (i == LEFT_HAND) {
                        homes[i] = HAND_HOME_POSITION;
                    } else {                                  
                        homes[i] = 180 - HAND_HOME_POSITION;  
                    }
                } else {

                    homes[i] = servo_[i].GetPosition();
                }
            } else {

                homes[i] = 90;
            }
        }

        MoveServos(500, homes);
        is_edarobot_resting_ = true;
    }

    vTaskDelay(pdMS_TO_TICKS(200));
}

bool EdaRobot::GetRestState() {
    return is_edarobot_resting_;
}

void EdaRobot::SetRestState(bool state) {
    is_edarobot_resting_ = state;
}

void EdaRobot::Jump(float steps, int period) {
    int up[SERVO_COUNT] = {90, 90, 150, 30, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    MoveServos(period, up);
    int down[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    MoveServos(period, down);
}

void EdaRobot::Walk(float steps, int period, int dir, int amount) {

    int A[SERVO_COUNT] = {30, 30, 30, 30, 0, 0};
    int O[SERVO_COUNT] = {0, 0, 5, -5, HAND_HOME_POSITION - 90, HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(dir * -90), DEG2RAD(dir * -90), 0, 0};

    if (amount > 0 && has_hands_) {

        A[LEFT_HAND] = amount;
        A[RIGHT_HAND] = amount;

        phase_diff[LEFT_HAND] = phase_diff[RIGHT_LEG];  
        phase_diff[RIGHT_HAND] = phase_diff[LEFT_LEG];  
    } else {
        A[LEFT_HAND] = 0;
        A[RIGHT_HAND] = 0;
    }

    Execute(A, O, period, phase_diff, steps);
}

void EdaRobot::Turn(float steps, int period, int dir, int amount) {

    int A[SERVO_COUNT] = {30, 30, 30, 30, 0, 0};
    int O[SERVO_COUNT] = {0, 0, 5, -5, HAND_HOME_POSITION - 90, HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(-90), DEG2RAD(-90), 0, 0};

    if (dir == LEFT) {
        A[0] = 30;  
        A[1] = 0;   
    } else {
        A[0] = 0;
        A[1] = 30;
    }

    if (amount > 0 && has_hands_) {

        A[LEFT_HAND] = amount;
        A[RIGHT_HAND] = amount;

        phase_diff[LEFT_HAND] = phase_diff[LEFT_LEG];    
        phase_diff[RIGHT_HAND] = phase_diff[RIGHT_LEG];  
    } else {
        A[LEFT_HAND] = 0;
        A[RIGHT_HAND] = 0;
    }

    Execute(A, O, period, phase_diff, steps);
}

void EdaRobot::Bend(int steps, int period, int dir) {

    int bend1[SERVO_COUNT] = {90, 90, 62, 35, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int bend2[SERVO_COUNT] = {90, 90, 62, 105, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int homes[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};

    if (dir == -1) {
        bend1[2] = 180 - 35;
        bend1[3] = 180 - 60;  
        bend2[2] = 180 - 105;
        bend2[3] = 180 - 60;
    }

    int T2 = 800;

    for (int i = 0; i < steps; i++) {
        MoveServos(T2 / 2, bend1);
        MoveServos(T2 / 2, bend2);
        vTaskDelay(pdMS_TO_TICKS(period * 0.8));
        MoveServos(500, homes);
    }
}

void EdaRobot::ShakeLeg(int steps, int period, int dir) {

    int numberLegMoves = 2;

    int shake_leg1[SERVO_COUNT] = {90, 90, 58, 35, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int shake_leg2[SERVO_COUNT] = {90, 90, 58, 120, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int shake_leg3[SERVO_COUNT] = {90, 90, 58, 60, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int homes[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};

    if (dir == 1) {
        shake_leg1[2] = 180 - 35;
        shake_leg1[3] = 180 - 58;
        shake_leg2[2] = 180 - 120;
        shake_leg2[3] = 180 - 58;
        shake_leg3[2] = 180 - 60;
        shake_leg3[3] = 180 - 58;
    }

    int T2 = 1000;

    period = period - T2;
    period = std::max(period, 200 * numberLegMoves);

    for (int j = 0; j < steps; j++) {

        MoveServos(T2 / 2, shake_leg1);
        MoveServos(T2 / 2, shake_leg2);

        for (int i = 0; i < numberLegMoves; i++) {
            MoveServos(period / (2 * numberLegMoves), shake_leg3);
            MoveServos(period / (2 * numberLegMoves), shake_leg2);
        }
        MoveServos(500, homes);  
    }

    vTaskDelay(pdMS_TO_TICKS(period));
}

void EdaRobot::UpDown(float steps, int period, int height) {

    int A[SERVO_COUNT] = {0, 0, height, height, 0, 0};
    int O[SERVO_COUNT] = {0, 0, height, -height, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(-90), DEG2RAD(90), 0, 0};

    Execute(A, O, period, phase_diff, steps);
}

void EdaRobot::Swing(float steps, int period, int height) {

    int A[SERVO_COUNT] = {0, 0, height, height, 0, 0};
    int O[SERVO_COUNT] = {
        0, 0, height / 2, -height / 2, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(0), DEG2RAD(0), 0, 0};

    Execute(A, O, period, phase_diff, steps);
}

void EdaRobot::TiptoeSwing(float steps, int period, int height) {

    int A[SERVO_COUNT] = {0, 0, height, height, 0, 0};
    int O[SERVO_COUNT] = {0, 0, height, -height, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, 0, 0};

    Execute(A, O, period, phase_diff, steps);
}

void EdaRobot::Jitter(float steps, int period, int height) {

    height = std::min(25, height);
    int A[SERVO_COUNT] = {height, height, 0, 0, 0, 0};
    int O[SERVO_COUNT] = {0, 0, 0, 0, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {DEG2RAD(-90), DEG2RAD(90), 0, 0, 0, 0};

    Execute(A, O, period, phase_diff, steps);
}

void EdaRobot::AscendingTurn(float steps, int period, int height) {

    height = std::min(13, height);
    int A[SERVO_COUNT] = {height, height, height, height, 0, 0};
    int O[SERVO_COUNT] = {
        0, 0, height + 4, -height + 4, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {DEG2RAD(-90), DEG2RAD(90), DEG2RAD(-90), DEG2RAD(90), 0, 0};

    Execute(A, O, period, phase_diff, steps);
}

void EdaRobot::Moonwalker(float steps, int period, int height, int dir) {

    int A[SERVO_COUNT] = {0, 0, height, height, 0, 0};
    int O[SERVO_COUNT] = {
        0, 0, height / 2 + 2, -height / 2 - 2, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int phi = -dir * 90;
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(phi), DEG2RAD(-60 * dir + phi), 0, 0};

    Execute(A, O, period, phase_diff, steps);
}

void EdaRobot::Crusaito(float steps, int period, int height, int dir) {
    int A[SERVO_COUNT] = {25, 25, height, height, 0, 0};
    int O[SERVO_COUNT] = {
        0, 0, height / 2 + 4, -height / 2 - 4, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {90, 90, DEG2RAD(0), DEG2RAD(-60 * dir), 0, 0};

    Execute(A, O, period, phase_diff, steps);
}

void EdaRobot::Flapping(float steps, int period, int height, int dir) {
    int A[SERVO_COUNT] = {12, 12, height, height, 0, 0};
    int O[SERVO_COUNT] = {
        0, 0, height - 10, -height + 10, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {
        DEG2RAD(0), DEG2RAD(180), DEG2RAD(-90 * dir), DEG2RAD(90 * dir), 0, 0};

    Execute(A, O, period, phase_diff, steps);
}

void EdaRobot::HandsUp(int period, int dir) {
    if (!has_hands_) {
        return;
    }

    int initial[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int target[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};

    if (dir == 0) {
        target[LEFT_HAND] = 170;
        target[RIGHT_HAND] = 10;
    } else if (dir == 1) {
        target[LEFT_HAND] = 170;
        target[RIGHT_HAND] = servo_[RIGHT_HAND].GetPosition();
    } else if (dir == -1) {
        target[RIGHT_HAND] = 10;
        target[LEFT_HAND] = servo_[LEFT_HAND].GetPosition();
    }

    MoveServos(period, target);
}

void EdaRobot::HandsDown(int period, int dir) {
    if (!has_hands_) {
        return;
    }

    int target[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};

    if (dir == 1) {
        target[RIGHT_HAND] = servo_[RIGHT_HAND].GetPosition();
    } else if (dir == -1) {
        target[LEFT_HAND] = servo_[LEFT_HAND].GetPosition();
    }

    MoveServos(period, target);
}

void EdaRobot::HandWave(int period, int dir) {
    if (!has_hands_) {
        return;
    }

    if (dir == BOTH) {
        HandWaveBoth(period);
        return;
    }

    int servo_index = (dir == LEFT) ? LEFT_HAND : RIGHT_HAND;

    int current_positions[SERVO_COUNT];
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            current_positions[i] = servo_[i].GetPosition();
        } else {
            current_positions[i] = 90;
        }
    }

    int position;
    if (servo_index == LEFT_HAND) {
        position = 170;
    } else {
        position = 10;
    }

    current_positions[servo_index] = position;
    MoveServos(300, current_positions);
    vTaskDelay(pdMS_TO_TICKS(300));

    for (int i = 0; i < 5; i++) {
        if (servo_index == LEFT_HAND) {
            current_positions[servo_index] = position - 30;
            MoveServos(period / 10, current_positions);
            vTaskDelay(pdMS_TO_TICKS(period / 10));
            current_positions[servo_index] = position + 30;
            MoveServos(period / 10, current_positions);
        } else {
            current_positions[servo_index] = position + 30;
            MoveServos(period / 10, current_positions);
            vTaskDelay(pdMS_TO_TICKS(period / 10));
            current_positions[servo_index] = position - 30;
            MoveServos(period / 10, current_positions);
        }
        vTaskDelay(pdMS_TO_TICKS(period / 10));
    }

    if (servo_index == LEFT_HAND) {
        current_positions[servo_index] = HAND_HOME_POSITION;
    } else {
        current_positions[servo_index] = 180 - HAND_HOME_POSITION;
    }
    MoveServos(300, current_positions);
}

void EdaRobot::HandWaveBoth(int period) {
    if (!has_hands_) {
        return;
    }

    int current_positions[SERVO_COUNT];
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            current_positions[i] = servo_[i].GetPosition();
        } else {
            current_positions[i] = 90;
        }
    }

    int left_position = 170;
    int right_position = 10;

    current_positions[LEFT_HAND] = left_position;
    current_positions[RIGHT_HAND] = right_position;
    MoveServos(300, current_positions);

    for (int i = 0; i < 5; i++) {

        current_positions[LEFT_HAND] = left_position - 30;
        current_positions[RIGHT_HAND] = right_position + 30;
        MoveServos(period / 10, current_positions);

        current_positions[LEFT_HAND] = left_position + 30;
        current_positions[RIGHT_HAND] = right_position - 30;
        MoveServos(period / 10, current_positions);
    }

    current_positions[LEFT_HAND] = HAND_HOME_POSITION;
    current_positions[RIGHT_HAND] = 180 - HAND_HOME_POSITION;
    MoveServos(300, current_positions);
}

void EdaRobot::EnableServoLimit(int diff_limit) {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].SetLimiter(diff_limit);
        }
    }
}

void EdaRobot::DisableServoLimit() {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].DisableLimiter();
        }
    }
}
