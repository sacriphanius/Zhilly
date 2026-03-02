#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include "led/circular_strip.h"

class LedStripControl {
private:
    CircularStrip* led_strip_;
    int brightness_level_;  

    int LevelToBrightness(int level) const;  
    StripColor RGBToColor(int red, int green, int blue);

public:
    explicit LedStripControl(CircularStrip* led_strip);
}; 

#endif 
