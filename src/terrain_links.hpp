#pragma once
#include "navigation.hpp"
namespace scape::nav {
inline Vec verticalPoint(Vec p,const Triangle& t){
 auto a=t.vertices[1]-t.vertices[0],b=t.vertices[2]-t.vertices[0];
 Vec n{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
 p.z=t.vertices[0].z-(n.x*(p.x-t.vertices[0].x)+n.y*(p.y-t.vertices[0].y))/n.z;return p;
}
inline std::uint64_t gridKey(int x,int y){return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x))<<32)|static_cast<std::uint32_t>(y);}
// Sample actual boundary edges rather than treating each broad triangle as one
// indivisible step. Links are directed and require a real lower landing surface.
template<class Validator>
std::size_t addTerrainLinks(std::vector<Triangle>& mesh,Vec focus,float radius,float maxDrop,Validator&& validate){
 std::unordered_map<std::uint64_t,std::vector<std::size_t>> grid;
 std::unordered_map<Key,std::size_t> indices;
 constexpr float cellSize=128.f;
 for(std::size_t i=0;i<mesh.size();++i){
  indices.emplace(mesh[i].key,i);auto& t=mesh[i];if(!walkable(t))continue;
  if(planarDistance(closest(focus,t),focus)>radius+200.f)continue;
  float minX=t.vertices[0].x,maxX=minX,minY=t.vertices[0].y,maxY=minY;
  for(auto p:t.vertices){minX=(std::min)(minX,p.x);maxX=(std::max)(maxX,p.x);minY=(std::min)(minY,p.y);maxY=(std::max)(maxY,p.y);}
  minX=(std::max)(minX,focus.x-radius-200);maxX=(std::min)(maxX,focus.x+radius+200);
  minY=(std::max)(minY,focus.y-radius-200);maxY=(std::min)(maxY,focus.y+radius+200);
  for(int x=static_cast<int>(std::floor(minX/cellSize));x<=static_cast<int>(std::floor(maxX/cellSize));++x)
   for(int y=static_cast<int>(std::floor(minY/cellSize));y<=static_cast<int>(std::floor(maxY/cellSize));++y)grid[gridKey(x,y)].push_back(i);
 }
 std::size_t added=0;
 for(auto& t:mesh){
  if(!walkable(t)||planarDistance(closest(focus,t),focus)>radius)continue;
  for(std::size_t edge=0;edge<3;++edge){
   auto neighbor=indices.find(t.neighbors[edge]);
   if(neighbor!=indices.end()&&t.traversal[edge]==Traversal::walk){
    auto mid=(t.vertices[edge]+t.vertices[(edge+1)%3])*.5f;
    if((closest(mid,mesh[neighbor->second])-mid).length()<=4.f)continue;
   }
   auto a=t.vertices[edge],b=t.vertices[(edge+1)%3],delta=b-a;
   float length=planarDistance(a,b);if(length<40.f)continue;
   Vec outward{-delta.y/length,delta.x/length,0};
   if(dot(outward,t.center()-(a+b)*.5f)>0)outward=outward*-1.f;
   int samples=static_cast<int>(std::ceil(length/48.f));
   for(int sample=0;sample<samples;++sample){
    auto rim=a+delta*((sample+.5f)/samples);if(planarDistance(rim,focus)>radius)continue;
    auto takeoff=verticalPoint(rim-outward*28.f,t);
    if((closest(takeoff,t)-takeoff).length()>2.f)continue;
    for(float reach:{40.f,72.f,104.f}){
     auto probe=rim+outward*reach;auto bin=grid.find(gridKey(static_cast<int>(std::floor(probe.x/cellSize)),static_cast<int>(std::floor(probe.y/cellSize))));if(bin==grid.end())continue;
     std::optional<Location> target;
     for(auto index:bin->second){
      const auto& landingTriangle=mesh[index];if(landingTriangle.key==t.key)continue;
      auto landing=verticalPoint(probe,landingTriangle);
      if((closest(landing,landingTriangle)-landing).length()>2.f)continue;
      if(!traversable(takeoff,landing,Traversal::drop,maxDrop))continue;
      // Select the highest supporting surface, never a floor beneath another.
      if(!target||landing.z>target->point.z)target=Location{index,landing};
     }
     if(!target)continue;
     bool duplicate=false;for(auto& link:t.links)if(link.target==mesh[target->triangle].key&&planarDistance(link.takeoff,takeoff)<80.f){duplicate=true;break;}
     if(duplicate)break;
     if(!validate(takeoff,target->point,Traversal::drop))continue;
     t.links.push_back({mesh[target->triangle].key,takeoff,target->point,Traversal::drop,maxDrop});++added;break;
    }
   }
  }
 }
 return added;
}
}
