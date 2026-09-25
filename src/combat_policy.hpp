#pragma once
#include <algorithm>
#include <cmath>
#include "control_math.hpp"
namespace scape::combat {
struct Motion {
 Vec position{},velocity{};double sampled{};bool valid{};
 void update(Vec next,double now){
  double dt=now-sampled;
  if(!valid||dt>1.||dt<0){position=next;velocity={};sampled=now;valid=true;return;}
  if(dt<.075)return;
  auto delta=next-position;auto speed=planarDistance(next,position)/static_cast<float>(dt);
  // Teleports, vertical traversal and stale samples are not chase velocity.
  if(speed>650||std::abs(delta.z)>64)velocity={};
  else{auto measured=delta*(1.f/static_cast<float>(dt));measured.z=0;velocity=measured*.7f+velocity*.3f;}
  position=next;sampled=now;
 }
 Vec intercept(Vec player,Vec target)const{
  float distance=planarDistance(player,target);
  if(distance<=125||!valid)return target;
  float horizon=std::clamp((distance-100.f)/350.f,.0f,.45f);
  auto lead=velocity*horizon;float length=lead.length();if(length>120)lead=lead*(120/length);
  return target+lead;
 }
};
inline bool shouldEvade(bool threat,bool defending,bool animationFree,bool grounded,float stamina){
 return threat&&!defending&&animationFree&&grounded&&std::isfinite(stamina)&&stamina>=10;
}
inline bool shouldRetarget(bool recentHostileHit,float attackerDistance,float targetDistance){
 return recentHostileHit&&attackerDistance<200&&targetDistance>220;
}
inline bool shouldBlock(bool threat,bool capable,bool held,float stamina,float maximum){
 if(!threat||!capable||!std::isfinite(stamina)||!std::isfinite(maximum))return false;
 return stamina>=(std::max)(held?10.f:20.f,maximum*(held?.10f:.20f));
}
inline bool shouldPower(bool threat,bool melee,bool ready,unsigned attacks,float stamina,float maximum){
 return !threat&&melee&&ready&&attacks>=3&&std::isfinite(stamina)&&std::isfinite(maximum)&&stamina>=(std::max)(60.f,maximum*.60f);
}
}
