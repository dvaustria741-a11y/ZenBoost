#pragma once
#include "Feature.h"
#include <vector>
#include <string>
#include <memory>

namespace FeatureManager {
    void init();
    void tick();                             // call every frame
    int  count();
    bool isEnabled(const std::string& id);
    void setEnabled(const std::string& id, bool on);
    void toggle(const std::string& id);
    std::vector<std::shared_ptr<Feature>>& getAll();
    std::shared_ptr<Feature> get(const std::string& id);
}