#pragma once
#include <string>
#include <functional>

struct Feature {
    std::string id;
    std::string label;
    std::string description;
    bool        enabled   = false;
    bool        hasTick   = false;
    std::function<void()> onEnable;
    std::function<void()> onDisable;
    std::function<void()> onTick;

    Feature(std::string id, std::string label, std::string desc)
        : id(std::move(id)), label(std::move(label)), description(std::move(desc)) {}
};