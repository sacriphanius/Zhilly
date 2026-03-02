#ifndef _LED_H_
#define _LED_H_

class Led {
public:
    virtual ~Led() = default;

    virtual void OnStateChanged() = 0;
};

class NoLed : public Led {
public:
    virtual void OnStateChanged() override {}
};

#endif 
