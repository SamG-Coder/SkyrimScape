#pragma once
#include "navigation.hpp"
#include <functional>
namespace scape::landscape {
// Sample loaded landscape on stable world coordinates. The centre divides each
// height-map square along both diagonals, preserving either native diagonal.
// Routing still uses grid::spacing and collision-validated circular clearance.
inline std::vector<nav::Triangle> sample(Vec focus,float range,
 const std::function<std::optional<float>(float,float)>& height){
 constexpr float step=128.f;
 const int x0=static_cast<int>(std::floor((focus.x-range)/step)),x1=static_cast<int>(std::ceil((focus.x+range)/step));
 const int y0=static_cast<int>(std::floor((focus.y-range)/step)),y1=static_cast<int>(std::ceil((focus.y+range)/step));
 const int width=x1-x0+1;std::vector<std::optional<float>> heights(width*(y1-y0+1));
 auto at=[&](int x,int y)->std::optional<float>&{return heights[(y-y0)*width+x-x0];};
 for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){auto z=height(x*step,y*step);if(z&&std::isfinite(*z))at(x,y)=z;}
 std::vector<nav::Triangle> result;
 for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){
  if(!at(x,y)||!at(x+1,y)||!at(x+1,y+1)||!at(x,y+1))continue;
  auto z=height((x+.5f)*step,(y+.5f)*step);if(!z||!std::isfinite(*z))continue;
  Vec center{(x+.5f)*step,(y+.5f)*step,*z};
  std::array<Vec,4> corners{{{x*step,y*step,*at(x,y)},{(x+1)*step,y*step,*at(x+1,y)},
   {(x+1)*step,(y+1)*step,*at(x+1,y+1)},{x*step,(y+1)*step,*at(x,y+1)}}};
  // Native navmesh keys occupy at most 48 bits. Landscape keys have bit 63.
  const auto id=(1ull<<63)|(static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)&0x3fffffff)<<32)|(static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)&0x3fffffff)<<2);
  for(unsigned edge=0;edge<4;++edge)result.push_back({id|edge,{{corners[edge],corners[(edge+1)%4],center}}});
 }return result;
}
}
