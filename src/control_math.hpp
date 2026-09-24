#pragma once
#include <algorithm>
#include <cmath>
namespace scape {
struct Vec {
 float x{},y{},z{};
 Vec operator+(Vec b)const{return {x+b.x,y+b.y,z+b.z};}
 Vec operator-(Vec b)const{return {x-b.x,y-b.y,z-b.z};}
 Vec operator*(float s)const{return {x*s,y*s,z*s};}
 float length()const{return std::sqrt(x*x+y*y+z*z);}
};
struct Orbit {
 float yaw{},pitch{.72f},distance{650.f};
 Vec right()const{return {std::cos(yaw),-std::sin(yaw),0};}
 Vec forward()const{return {std::sin(yaw)*std::cos(pitch),std::cos(yaw)*std::cos(pitch),-std::sin(pitch)};}
 Vec up()const{return {std::sin(yaw)*std::sin(pitch),std::cos(yaw)*std::sin(pitch),std::cos(pitch)};}
 Vec position(Vec focus)const{return focus-forward()*distance;}
 void rotate(float dx,float dy){yaw=std::remainder(yaw+dx*.005f,6.283185307f);pitch=std::clamp(pitch+dy*.005f,.2f,1.35f);}
 void zoom(float steps){distance=std::clamp(distance*std::exp(-steps*.12f),90.f,4000.f);}
};
inline float heading(Vec from,Vec to){return std::atan2(to.x-from.x,to.y-from.y);}
struct MoveInput{float x{},y{};};
inline MoveInput cameraRelativeInput(float desiredHeading,float nativeCameraHeading){
 const auto angle=desiredHeading-nativeCameraHeading;
 return {std::sin(angle),std::cos(angle)};
}
inline bool jumpLanding(Vec takeoff,Vec landing){
 const auto horizontal=std::hypot(landing.x-takeoff.x,landing.y-takeoff.y);
 return horizontal>=50.f&&horizontal<=170.f&&landing.z-takeoff.z<=60.f&&landing.z-takeoff.z>=-70.f;
}
inline float planarDistance(Vec a,Vec b){return std::hypot(a.x-b.x,a.y-b.y);}
inline bool reached(Vec a,Vec b,float radius){return planarDistance(a,b)<=radius&&std::abs(a.z-b.z)<=100.f;}
inline bool passedWaypoint(Vec position,Vec from,Vec to){
 auto segment=to-from;segment.z=0;auto offset=position-to;offset.z=0;
 float length=segment.length();if(length<1.f)return false;
 const float along=(offset.x*segment.x+offset.y*segment.y)/length;
 const float lateral=std::abs(offset.x*segment.y-offset.y*segment.x)/length;
 return along>=0&&along<=45.f&&lateral<=24.f&&std::abs(position.z-to.z)<=100.f;
}
// Wheel pulses may carry held duration; only the release (value zero) is ignored.
inline float wheelStep(unsigned id,float value){return value>0?(id==8?1.f:id==9?-1.f:0.f):0.f;}
struct ClickAnimation {
 float radius{},alpha{};
 bool visible{};
};
inline ClickAnimation clickAnimation(float ageSeconds){
 if(ageSeconds<0||ageSeconds>=.6f)return {};
 float t=ageSeconds/.6f;
 return {3.f+5.f*t,100.f*(1.f-t),true};
}
}
