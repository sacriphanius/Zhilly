#include "otto_movements.h"

#include <algorithm>

#include "freertos/idf_additions.h"
#include "oscillator.h"

static const char* TAG = "OttoMovements";

#define HAND_HOME_POSITION 45

Otto::Otto() {
    is_otto_resting_ = false;
    has_hands_ = false;

    for (int i = 0; i < SERVO_COUNT; i++) {
        servo_pins_[i] = -1;
        servo_trim_[i] = 0;
    }
}

Otto::~Otto() {
    DetachServos();
}

unsigned long IRAM_ATTR millis() {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

void Otto::Init(int left_leg, int right_leg, int left_foot, int right_foot, int left_hand,
                int right_hand) {
    servo_pins_[LEFT_LEG] = left_leg;
    servo_pins_[RIGHT_LEG] = right_leg;
    servo_pins_[LEFT_FOOT] = left_foot;
    servo_pins_[RIGHT_FOOT] = right_foot;
    servo_pins_[LEFT_HAND] = left_hand;
    servo_pins_[RIGHT_HAND] = right_hand;

    has_hands_ = (left_hand != -1 && right_hand != -1);

    AttachServos();
    is_otto_resting_ = false;
}

void Otto::AttachServos() {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].Attach(servo_pins_[i]);
        }
    }
}

void Otto::DetachServos() {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].Detach();
        }
    }
}

void Otto::SetTrims(int left_leg, int right_leg, int left_foot, int right_foot, int left_hand,
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

void Otto::MoveServos(int time, int servo_target[]) {
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

void Otto::MoveSingle(int position, int servo_number) {
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

void Otto::OscillateServos(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
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

void Otto::Execute(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
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

void Otto::Execute2(int amplitude[SERVO_COUNT], int center_angle[SERVO_COUNT], int period,
                    double phase_diff[SERVO_COUNT], float steps = 1.0) {
    if (GetRestState() == true) {
        SetRestState(false);
    }

    int offset[SERVO_COUNT];
    for (int i = 0; i < SERVO_COUNT; i++) {
        offset[i] = center_angle[i] - 90;
    }

    int cycles = (int)steps;

    if (cycles >= 1)
        for (int i = 0; i < cycles; i++)
            OscillateServos(amplitude, offset, period, phase_diff);

    OscillateServos(amplitude, offset, period, phase_diff, (float)steps - cycles);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void Otto::Home(bool hands_down) {
    if (is_otto_resting_ == false) {

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

        MoveServos(700, homes);
        is_otto_resting_ = true;
    }

    vTaskDelay(pdMS_TO_TICKS(200));
}

bool Otto::GetRestState() {
    return is_otto_resting_;
}

void Otto::SetRestState(bool state) {
    is_otto_resting_ = state;
}

void Otto::Jump(float steps, int period) {
    int up[SERVO_COUNT] = {90, 90, 150, 30, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    MoveServos(period, up);
    int down[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    MoveServos(period, down);
}

void Otto::Walk(float steps, int period, int dir, int amount) {

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

void Otto::Turn(float steps, int period, int dir, int amount) {

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

void Otto::Bend(int steps, int period, int dir) {

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

void Otto::ShakeLeg(int steps, int period, int dir) {

    int numberLegMoves = 2;

    int shake_leg1[SERVO_COUNT] = {90, 90, 58, 35, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int shake_leg2[SERVO_COUNT] = {90, 90, 58, 120, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int shake_leg3[SERVO_COUNT] = {90, 90, 58, 60, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int homes[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};

    if (dir == LEFT) {
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

void Otto::Sit() {
    int target[SERVO_COUNT] = {120, 60, 0, 180, 45, 135};
    MoveServos(600, target);
}

void Otto::UpDown(float steps, int period, int height) {

    int A[SERVO_COUNT] = {0, 0, height, height, 0, 0};
    int O[SERVO_COUNT] = {0, 0, height, -height, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(-90), DEG2RAD(90), 0, 0};

    Execute(A, O, period, phase_diff, steps);
}

void Otto::Swing(float steps, int period, int height) {

    int A[SERVO_COUNT] = {0, 0, height, height, 0, 0};
    int O[SERVO_COUNT] = {
        0, 0, height / 2, -height / 2, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(0), DEG2RAD(0), 0, 0};

    Execute(A, O, period, phase_diff, steps);
}

void Otto::TiptoeSwing(float steps, int period, int height) {

    int A[SERVO_COUNT] = {0, 0, height, height, 0, 0};
    int O[SERVO_COUNT] = {0, 0, height, -height, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, 0, 0};

    Execute(A, O, period, phase_diff, steps);
}

void Otto::Jitter(float steps, int period, int height) {

    height = std::min(25, height);
    int A[SERVO_COUNT] = {height, height, 0, 0, 0, 0};
    int O[SERVO_COUNT] = {0, 0, 0, 0, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {DEG2RAD(-90), DEG2RAD(90), 0, 0, 0, 0};

    Execute(A, O, period, phase_diff, steps);
}

void Otto::AscendingTurn(float steps, int period, int height) {

    height = std::min(13, height);
    int A[SERVO_COUNT] = {height, height, height, height, 0, 0};
    int O[SERVO_COUNT] = {
        0, 0, height + 4, -height + 4, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {DEG2RAD(-90), DEG2RAD(90), DEG2RAD(-90), DEG2RAD(90), 0, 0};

    Execute(A, O, period, phase_diff, steps);
}

void Otto::Moonwalker(float steps, int period, int height, int dir) {

    int A[SERVO_COUNT] = {0, 0, height, height, 0, 0};
    int O[SERVO_COUNT] = {
        0, 0, height / 2 + 2, -height / 2 - 2, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int phi = -dir * 90;
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(phi), DEG2RAD(-60 * dir + phi), 0, 0};

    Execute(A, O, period, phase_diff, steps);
}

void Otto::Crusaito(float steps, int period, int height, int dir) {
    int A[SERVO_COUNT] = {25, 25, height, height, 0, 0};
    int O[SERVO_COUNT] = {
        0, 0, height / 2 + 4, -height / 2 - 4, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {90, 90, DEG2RAD(0), DEG2RAD(-60 * dir), 0, 0};

    Execute(A, O, period, phase_diff, steps);
}

void Otto::Flapping(float steps, int period, int height, int dir) {
    int A[SERVO_COUNT] = {12, 12, height, height, 0, 0};
    int O[SERVO_COUNT] = {
        0, 0, height - 10, -height + 10, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {
        DEG2RAD(0), DEG2RAD(180), DEG2RAD(-90 * dir), DEG2RAD(90 * dir), 0, 0};

    Execute(A, O, period, phase_diff, steps);
}

void Otto::WhirlwindLeg(float steps, int period, int amplitude) {

    int target[SERVO_COUNT] = {90, 90, 180, 90, 45, 20};
    MoveServos(100, target);
    target[RIGHT_FOOT] = 160;
    MoveServos(500, target);
    vTaskDelay(pdMS_TO_TICKS(1000));

    int C[SERVO_COUNT] = {90, 90, 180, 160, 45, 20};
    int A[SERVO_COUNT] = {amplitude, 0, 0, 0, amplitude, 0};
    double phase_diff[SERVO_COUNT] = {DEG2RAD(20), 0, 0, 0, DEG2RAD(20), 0};
    Execute2(A, C, period, phase_diff, steps);

}

void Otto::HandsUp(int period, int dir) {
    if (!has_hands_) {
        return;
    }

    int target[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};

    if (dir == 0) {
        target[LEFT_HAND] = 170;
        target[RIGHT_HAND] = 10;
    } else if (dir == LEFT) {
        target[LEFT_HAND] = 170;
        target[RIGHT_HAND] = servo_[RIGHT_HAND].GetPosition();
    } else if (dir == RIGHT) {
        target[RIGHT_HAND] = 10;
        target[LEFT_HAND] = servo_[LEFT_HAND].GetPosition();
    }

    MoveServos(period, target);
}

void Otto::HandsDown(int period, int dir) {
    if (!has_hands_) {
        return;
    }

    int target[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};

    if (dir == LEFT) {
        target[RIGHT_HAND] = servo_[RIGHT_HAND].GetPosition();
    } else if (dir == RIGHT) {
        target[LEFT_HAND] = servo_[LEFT_HAND].GetPosition();
    }

    MoveServos(period, target);
}

void Otto::HandWave(int dir) {
    if (!has_hands_) {
        return;
    }
    if (dir == LEFT) {
        int center_angle[SERVO_COUNT] = {90, 90, 90, 90, 160, 135};
        int A[SERVO_COUNT] = {0, 0, 0, 0, 20, 0};
        double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, DEG2RAD(90), 0};
        Execute2(A, center_angle, 300, phase_diff, 5);
    }
    else if (dir == RIGHT) {
        int center_angle[SERVO_COUNT] = {90, 90, 90, 90, 45, 20};
        int A[SERVO_COUNT] = {0, 0, 0, 0, 0, 20};
        double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, 0, DEG2RAD(90)};
        Execute2(A, center_angle, 300, phase_diff, 5);
    }
    else {
        int center_angle[SERVO_COUNT] = {90, 90, 90, 90, 160, 20};
        int A[SERVO_COUNT] = {0, 0, 0, 0, 20, 20};
        double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, DEG2RAD(90), DEG2RAD(90)};
        Execute2(A, center_angle, 300, phase_diff, 5);
    }
}

void Otto::Windmill(float steps, int period, int amplitude) {
    if (!has_hands_) {
        return;
    }

    int center_angle[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
    int A[SERVO_COUNT] = {0, 0, 0, 0, amplitude, amplitude};
    double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, DEG2RAD(90), DEG2RAD(90)};
    Execute2(A, center_angle, period, phase_diff, steps);
}

void Otto::Takeoff(float steps, int period, int amplitude) {
    if (!has_hands_) {
        return;
    }

    Home(true);

    int center_angle[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
    int A[SERVO_COUNT] = {0, 0, 0, 0, amplitude, amplitude};
    double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, DEG2RAD(90), DEG2RAD(-90)};
    Execute2(A, center_angle, period, phase_diff, steps);
}

void Otto::Fitness(float steps, int period, int amplitude) {
    if (!has_hands_) {
        return;
    }
    int target[SERVO_COUNT] = {90, 90, 90, 0, 160, 135};
    MoveServos(100, target);
    target[LEFT_FOOT] = 20;
    MoveServos(400, target);
    vTaskDelay(pdMS_TO_TICKS(2000));

    int C[SERVO_COUNT] = {90, 90, 20, 90, 160, 135};
    int A[SERVO_COUNT] = {0, 0, 0, 0, 0, amplitude};
    double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, 0, 0};
    Execute2(A, C, period, phase_diff, steps);

}

void Otto::Greeting(int dir, float steps) {
    if (!has_hands_) {
        return;
    }
    if (dir == LEFT) {
        int target[SERVO_COUNT] = {90, 90, 150, 150, 45, 135};
        MoveServos(400, target);
        int C[SERVO_COUNT] = {90, 90, 150, 150, 160, 135};
        int A[SERVO_COUNT] = {0, 0, 0, 0, 20, 0};
        double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, 0, 0};
        Execute2(A, C, 300, phase_diff, steps);
    }
    else if (dir == RIGHT) {
        int target[SERVO_COUNT] = {90, 90, 30, 30, 45, 135};
        MoveServos(400, target);
        int C[SERVO_COUNT] = {90, 90, 30, 30, 45, 20};
        int A[SERVO_COUNT] = {0, 0, 0, 0, 0, 20};
        double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, 0, 0};
        Execute2(A, C, 300, phase_diff, steps);
    }

}

void Otto::Shy(int dir, float steps) {
    if (!has_hands_) {
        return;
    }

    if (dir == LEFT) {
        int target[SERVO_COUNT] = {90, 90, 150, 150, 45, 135};
        MoveServos(400, target);
        int C[SERVO_COUNT] = {90, 90, 150, 150, 45, 135};
        int A[SERVO_COUNT] = {0, 0, 0, 0, 20, 20};
        double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, DEG2RAD(90), DEG2RAD(-90)};
        Execute2(A, C, 300, phase_diff, steps);
    }
    else if (dir == RIGHT) {
        int target[SERVO_COUNT] = {90, 90, 30, 30, 45, 135};
        MoveServos(400, target);
        int C[SERVO_COUNT] = {90, 90, 30, 30, 45, 135};
        int A[SERVO_COUNT] = {0, 0, 0, 0, 0, 20};
        double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, DEG2RAD(90), DEG2RAD(-90)};
        Execute2(A, C, 300, phase_diff, steps);
    }
}

void Otto::RadioCalisthenics() {
    if (!has_hands_) {
        return;
    }

    const int period = 1000;
    const float steps = 8.0;

    int C1[SERVO_COUNT] = {90, 90, 90, 90, 145, 45};
    int A1[SERVO_COUNT] = {0, 0, 0, 0, 45, 45};
    double phase_diff1[SERVO_COUNT] = {0, 0, 0, 0, DEG2RAD(90), DEG2RAD(-90)};
    Execute2(A1, C1, period, phase_diff1, steps);

    int C2[SERVO_COUNT] = {90, 90, 115, 65, 90, 90};
    int A2[SERVO_COUNT] = {0, 0, 25, 25, 0, 0};
    double phase_diff2[SERVO_COUNT] = {0, 0, DEG2RAD(90), DEG2RAD(-90), 0, 0};
    Execute2(A2, C2, period, phase_diff2, steps);

    int C3[SERVO_COUNT] = {90, 90, 130, 130, 90, 90};
    int A3[SERVO_COUNT] = {0, 0, 0, 0, 20, 0};
    double phase_diff3[SERVO_COUNT] = {0, 0, 0, 0, 0, 0};
    Execute2(A3, C3, period, phase_diff3, steps);

    int C4[SERVO_COUNT] = {90, 90, 50, 50, 90, 90};
    int A4[SERVO_COUNT] = {0, 0, 0, 0, 0, 20};
    double phase_diff4[SERVO_COUNT] = {0, 0, 0, 0, 0, 0};
    Execute2(A4, C4, period, phase_diff4, steps);
}

void Otto::MagicCircle() {
    if (!has_hands_) {
        return;
    }

    int A[SERVO_COUNT] = {30, 30, 30, 30, 50, 50};
    int O[SERVO_COUNT] = {0, 0, 5, -5, 0, 0};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(-90), DEG2RAD(-90), DEG2RAD(-90) , DEG2RAD(90)};

    Execute(A, O, 700, phase_diff, 40);
}

void Otto::Showcase() {
    if (GetRestState() == true) {
        SetRestState(false);
    }

    Walk(3, 1000, FORWARD, 50);
    vTaskDelay(pdMS_TO_TICKS(500));

    if (has_hands_) {
        HandWave(LEFT);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (has_hands_) {
        RadioCalisthenics();
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    Moonwalker(3, 900, 25, LEFT);
    vTaskDelay(pdMS_TO_TICKS(500));

    Swing(3, 1000, 30);
    vTaskDelay(pdMS_TO_TICKS(500));

    if (has_hands_) {
        Takeoff(5, 300, 40);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (has_hands_) {
        Fitness(5, 1000, 25);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    Walk(3, 1000, BACKWARD, 50);
}

void Otto::EnableServoLimit(int diff_limit) {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].SetLimiter(diff_limit);
        }
    }
}

void Otto::DisableServoLimit() {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].DisableLimiter();
        }
    }
}