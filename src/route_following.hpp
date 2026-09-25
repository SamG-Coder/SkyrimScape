#pragma once
#include "navigation.hpp"
namespace scape {
inline bool sameWalkDirection(Vec position,Vec next,Vec destination){
 auto a=next-position,b=destination-position;float lengths=std::hypot(a.x,a.y)*std::hypot(b.x,b.y);
 return lengths>1.f&&(a.x*b.x+a.y*b.y)/lengths>.5f;
}
// Join a finished background search ahead of the moving character, without
// skipping traversal actions or trusting a stale start position.
template<class Clear>bool joinMovingRoute(nav::Route& route,Vec position,Clear clear){
 std::size_t join=0;
 for(std::size_t i=1;i<route.points.size();++i){
  if(route.traversal[i]!=nav::Traversal::walk)break;
  if(clear(position,route.points[i]))join=i;
  else break;
  if(planarDistance(position,route.points[i])>700.f)break;
 }
 if(!join)return false;
 route.points.erase(route.points.begin(),route.points.begin()+join);
 route.traversal.erase(route.traversal.begin(),route.traversal.begin()+join);
 route.points.insert(route.points.begin(),position);route.traversal.insert(route.traversal.begin(),nav::Traversal::walk);return true;
}
}
