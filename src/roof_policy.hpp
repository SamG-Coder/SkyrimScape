#pragma once
#include "control_math.hpp"
#include <string_view>
#include <string>
namespace scape::roof {
inline bool named(std::string_view value){
 std::string name(value);for(auto& c:name)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
 return name.find("roof")!=name.npos||name.find("ceiling")!=name.npos||name.find("rafter")!=name.npos;
}
// Keep straddling pieces: hiding them would remove stairs/floors below the cut.
inline bool aboveInteriorCut(Vec center,float radius,float feetZ){
 return std::isfinite(center.z)&&std::isfinite(radius)&&std::isfinite(feetZ)&&
  radius>0&&center.z-radius>feetZ+180.f;
}
inline bool obstructs(Vec center,float radius,Vec focus,Vec camera){
 if(!std::isfinite(radius)||radius<=0||radius>1400||center.z<focus.z+40||camera.z<=focus.z+40)return false;
 auto direction=camera-focus;float length2=direction.x*direction.x+direction.y*direction.y+direction.z*direction.z;
 if(length2<1)return false;
 auto offset=center-focus;float t=(offset.x*direction.x+offset.y*direction.y+offset.z*direction.z)/length2;
 if(t<=0||t>=1)return false;
 auto closest=focus+direction*t;
 return (center-closest).length()<=radius+24;
}
}
