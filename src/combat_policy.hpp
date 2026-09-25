#pragma once
#include <algorithm>
#include <cmath>
#include "control_math.hpp"
namespace scape::combat {
inline bool inMeleeRange(Vec from,Vec target,float reach){
 return std::isfinite(reach)&&reach>0&&std::abs(target.z-from.z)<=64.f&&(target-from).length()<=reach;
}
struct MeleeRange {float strike,approach;};
inline MeleeRange meleeRange(float nativeReach){
 // Invalid native values must not allow attacks from arbitrarily far away.
 if(!std::isfinite(nativeReach)||nativeReach<=0||nativeReach>2000)nativeReach=64.f;
 float strike=(std::max)(8.f,nativeReach-(std::min)(18.f,nativeReach*.15f));
 return {strike,(std::max)(1.f,strike-20.f)};
}
inline float lightHoldSeconds(float powerDelay){
 if(!std::isfinite(powerDelay)||powerDelay<.1f||powerDelay>2.f)powerDelay=.5f;
 return (std::min)(.12f,powerDelay*.5f);
}
enum class RangedChoice { keepFighting, closeShooter, seekCover };
inline RangedChoice rangedChoice(float shooterDistance,float targetDistance,float healthFraction,float staminaFraction){
 if(!std::isfinite(shooterDistance)||!std::isfinite(healthFraction)||!std::isfinite(staminaFraction))return RangedChoice::keepFighting;
 if(shooterDistance<220&&healthFraction>.35f)return RangedChoice::closeShooter;
 if(healthFraction<.5f||staminaFraction<.25f)return RangedChoice::seekCover;
 if(targetDistance<=130)return RangedChoice::keepFighting;
 return shooterDistance>500?RangedChoice::seekCover:RangedChoice::closeShooter;
}
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
 Vec intercept(Vec player,Vec target,float strikeRange=110.f)const{
  float distance=planarDistance(player,target);
  if(distance<=strikeRange+15.f||!valid)return target;
  float horizon=std::clamp((distance-strikeRange)/350.f,.0f,.45f);
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
