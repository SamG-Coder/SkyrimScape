#pragma once
#include <algorithm>
#include <cmath>
namespace scape::combat {
inline bool shouldBlock(bool threat,bool capable,bool held,float stamina,float maximum){
 if(!threat||!capable||!std::isfinite(stamina)||!std::isfinite(maximum))return false;
 return stamina>=(std::max)(held?10.f:20.f,maximum*(held?.10f:.20f));
}
inline bool shouldPower(bool threat,bool melee,bool ready,unsigned attacks,float stamina,float maximum){
 return !threat&&melee&&ready&&attacks>=3&&std::isfinite(stamina)&&std::isfinite(maximum)&&stamina>=(std::max)(60.f,maximum*.60f);
}
}
