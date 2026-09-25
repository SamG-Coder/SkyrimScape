#pragma once
#include "control_math.hpp"
#include <cstdint>
namespace runtime_style {
void install();
void setGameplay(bool active);
void toggle();
void toggleRoofs();
void updateRoofView(bool active,scape::Vec focus,scape::Vec camera,bool interior=false);
}
