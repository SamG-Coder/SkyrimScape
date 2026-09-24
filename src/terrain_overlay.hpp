#pragma once
#include "navigation.hpp"
namespace scape::nav {
// 0 = disconnected; 1 = walking connection; 2 = requires a jump/drop link.
inline std::vector<unsigned char> reachability(const std::vector<Triangle>& mesh,Vec position){
 std::vector<unsigned char> result(mesh.size());auto start=locate(mesh,position,160.f);if(!start)return result;
 std::unordered_map<Key,std::size_t> indices;for(std::size_t i=0;i<mesh.size();++i)indices.emplace(mesh[i].key,i);
 for(int pass=1;pass<=2;++pass){
  std::vector<std::size_t> queue{start->triangle};std::vector<bool> seen(mesh.size());seen[start->triangle]=true;
  for(std::size_t cursor=0;cursor<queue.size();++cursor){
   auto index=queue[cursor];if(!result[index])result[index]=static_cast<unsigned char>(pass);
   auto& t=mesh[index];
   for(std::size_t edge=0;edge<3;++edge){
    auto neighbor=indices.find(t.neighbors[edge]);if(neighbor==indices.end()||seen[neighbor->second])continue;
    auto& next=mesh[neighbor->second];if(!walkable(next))continue;
    if(t.traversal[edge]==Traversal::walk){
     auto midpoint=(t.vertices[edge]+t.vertices[(edge+1)%3])*.5f;
     if((closest(midpoint,next)-midpoint).length()>4.f)continue;
    }else{
     if(pass==1)continue;
     auto [from,to]=traversalEndpoints(t,edge,next);if(!traversable(from,to,t.traversal[edge]))continue;
    }
    seen[neighbor->second]=true;queue.push_back(neighbor->second);
   }
   if(pass==2)for(const auto& link:t.links){
    auto found=indices.find(link.target);if(found==indices.end()||seen[found->second])continue;
    if(!traversable(link.takeoff,link.landing,link.type,link.dropLimit))continue;
    seen[found->second]=true;queue.push_back(found->second);
   }
  }
 }
 return result;
}
struct ScreenPoint{float x,y;};
inline std::vector<ScreenPoint> clipScreen(std::vector<ScreenPoint> polygon){
 for(int edge=0;edge<4&&!polygon.empty();++edge){
  auto distance=[edge](ScreenPoint p){return edge==0?p.x:edge==1?1-p.x:edge==2?p.y:1-p.y;};
  std::vector<ScreenPoint> output;auto previous=polygon.back();float before=distance(previous);
  for(auto current:polygon){
   float after=distance(current);
   if((before>=0)!=(after>=0)){float t=before/(before-after);output.push_back({previous.x+t*(current.x-previous.x),previous.y+t*(current.y-previous.y)});}
   if(after>=0)output.push_back(current);previous=current;before=after;
  }
  polygon=std::move(output);
 }
 return polygon;
}
}
