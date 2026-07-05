#pragma once
namespace InputHook {
    void install();
    // Called internally; ZenGui reads these values
    float getLastTouchX();
    float getLastTouchY();
    bool  getTouchDown();
}