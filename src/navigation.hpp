#pragma once
#include "control_math.hpp"
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>
namespace scape::nav {
using Key=std::uint64_t;
constexpr Key noKey=(std::numeric_limits<Key>::max)();
enum class Traversal:unsigned char{walk,jump,drop};
struct TerrainLink {Key target{noKey};Vec takeoff,landing;Traversal type{Traversal::drop};float dropLimit{180.f};};
struct Triangle {
 Key key{};
 std::array<Vec,3> vertices;
 std::array<Key,3> neighbors{noKey,noKey,noKey};
 std::array<Traversal,3> traversal{};
 std::vector<TerrainLink> links;
 Vec center()const{return (vertices[0]+vertices[1]+vertices[2])*(1.f/3.f);}
};
inline float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec closest(Vec p,const Triangle& t){
 auto a=t.vertices[0],b=t.vertices[1],c=t.vertices[2];
 auto ab=b-a,ac=c-a,ap=p-a;
 float d1=dot(ab,ap),d2=dot(ac,ap);if(d1<=0&&d2<=0)return a;
 auto bp=p-b;float d3=dot(ab,bp),d4=dot(ac,bp);if(d3>=0&&d4<=d3)return b;
 float vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return a+ab*(d1/(d1-d3));
 auto cp=p-c;float d5=dot(ab,cp),d6=dot(ac,cp);if(d6>=0&&d5<=d6)return c;
 float vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return a+ac*(d2/(d2-d6));
 float va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0)return b+(c-b)*((d4-d3)/((d4-d3)+(d5-d6)));
 float sum=va+vb+vc;
 // Malformed zero-area triangles do not produce a NaN route.
 if(std::abs(sum)<1e-10f)return a;
 return a+ab*(vb/sum)+ac*(vc/sum);
}
inline bool walkable(const Triangle& t){
 auto a=t.vertices[1]-t.vertices[0],b=t.vertices[2]-t.vertices[0];
 Vec n{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
 return std::isfinite(n.length())&&n.length()>1e-3f&&std::abs(n.z)/n.length()>.65f;
}
struct Location{std::size_t triangle;Vec point;};
inline std::optional<Location> locate(const std::vector<Triangle>& triangles,Vec p,float limit){
 std::optional<Location> result;float distance=limit;
 for(std::size_t i=0;i<triangles.size();++i){
  if(!walkable(triangles[i]))continue;
  auto q=closest(p,triangles[i]);float d=(q-p).length();
  if(d<=distance){distance=d;result=Location{i,q};}
 }
 return result;
}
// Project vertically onto a walkable surface, keeping horizontal and height
// tolerances separate so an elevated collision hit can resolve to ground.
inline std::optional<Location> surface(const std::vector<Triangle>& triangles,Vec p,float horizontal=48.f,float below=600.f,float above=40.f){
 std::optional<Location> result;float best=std::numeric_limits<float>::infinity();
 for(std::size_t i=0;i<triangles.size();++i){
  const auto& t=triangles[i];if(!walkable(t))continue;
  auto a=t.vertices[1]-t.vertices[0],b=t.vertices[2]-t.vertices[0];
  Vec n{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
  Vec projected{p.x,p.y,t.vertices[0].z-(n.x*(p.x-t.vertices[0].x)+n.y*(p.y-t.vertices[0].y))/n.z};
  auto q=closest(projected,t);float lateral=planarDistance(q,p),height=q.z-p.z;
  if(lateral>horizontal||height>above||height < -below)continue;
  float score=std::abs(height)+lateral*4.f;
  if(score<best){best=score;result=Location{i,q};}
 }
 return result;
}
inline bool traversable(Vec takeoff,Vec landing,Traversal type,float dropLimit=180.f){
 float height=landing.z-takeoff.z,distance=planarDistance(takeoff,landing);
 if(distance<8.f||distance>180.f)return false;
 if(type==Traversal::jump)return height>=-30.f&&height<=65.f;
 if(type==Traversal::drop)return height < -20.f&&height>=-dropLimit&&distance<=140.f;
 return false;
}
inline std::pair<Vec,Vec> traversalEndpoints(const Triangle& from,std::size_t edge,const Triangle& to){
 auto takeoff=(from.vertices[edge]+from.vertices[(edge+1)%3])*.5f;
 auto landing=closest(takeoff,to);
 auto inward=from.center()-takeoff;float d=inward.length();if(d>0)takeoff=takeoff+inward*((std::min)(28.f,d)/d);
 inward=to.center()-landing;d=inward.length();if(d>0)landing=landing+inward*((std::min)(28.f,d)/d);
 return {takeoff,landing};
}
struct Route{std::vector<Vec> points;std::size_t expanded{};std::vector<Traversal> traversal;};
inline bool corridorOnMesh(const std::vector<Triangle>& triangles,Vec from,Vec to){
 auto delta=to-from;float length=delta.length();if(length<1.f)return true;
 // Limit work and shortcut length, and keep room for a character rather than a point.
 if(length>1536.f)return false;
 auto side=Vec{-delta.y,delta.x,0};float width=side.length();
 if(width>.01f)side=side*(14.f/width);
 int count=static_cast<int>(std::ceil(length/24.f));
 for(int i=1;i<count;++i){
  auto p=from+delta*(static_cast<float>(i)/count);
  for(float offset:{-1.f,0.f,1.f})if(!locate(triangles,p+side*offset,8.f))return false;
 }
 return true;
}
inline void smooth(Route& route,const std::vector<Triangle>& corridor){
 if(route.points.size()<3)return;
 if(route.traversal.size()!=route.points.size())route.traversal.resize(route.points.size());
 std::vector<Vec> result{route.points.front()};std::vector<Traversal> types{Traversal::walk};std::size_t cursor=0;
 while(cursor+1<route.points.size()){
  std::size_t next=cursor+1;
  auto end=(std::min)(route.points.size()-1,cursor+64);
  for(auto i=cursor+1;i<=end;++i)if(route.traversal[i]!=Traversal::walk){end=i-1;break;}
  for(auto candidate=end;candidate>cursor+1;--candidate)
   if(corridorOnMesh(corridor,route.points[cursor],route.points[candidate])){next=candidate;break;}
  result.push_back(route.points[next]);types.push_back(route.traversal[next]);cursor=next;
 }
 route.points=std::move(result);
 route.traversal=std::move(types);
}
inline Route plan(const std::vector<Triangle>& triangles,Vec start,Vec finish,float endpointLimit=160.f){
 Route result;auto from=locate(triangles,start,endpointLimit),to=locate(triangles,finish,endpointLimit);
 if(!from||!to)return result;
 std::unordered_map<Key,std::size_t> indices;
 for(std::size_t i=0;i<triangles.size();++i)indices.emplace(triangles[i].key,i);
 struct Entry{float priority;float cost;std::size_t index;bool operator<(const Entry& b)const{return priority>b.priority;}};
 std::priority_queue<Entry> queue;const auto absent=triangles.size();
 std::vector<float> costs(absent,std::numeric_limits<float>::infinity());
 std::vector<std::size_t> parent(absent,absent);std::vector<Vec> portals(absent);
 std::vector<Vec> landings(absent);std::vector<Traversal> actions(absent);
 std::vector<Vec> entries(absent);entries[from->triangle]=from->point;
 costs[from->triangle]=0;queue.push({0,0,from->triangle});
 while(!queue.empty()&&result.expanded<50000){
  auto current=queue.top();queue.pop();if(current.cost>costs[current.index])continue;++result.expanded;
  if(current.index==to->triangle){
   std::vector<std::size_t> chain;
   for(auto index=current.index;index!=from->triangle;index=parent[index]){if(index==absent)return {};chain.push_back(index);}
   result.points.push_back(from->point);
   // Consecutive shared-edge points lie in the same convex triangle. There is
   // no need to detour to its center between crossing points.
   result.traversal.resize(result.points.size());
   for(auto it=chain.rbegin();it!=chain.rend();++it){
    result.points.push_back(portals[*it]);result.traversal.push_back(Traversal::walk);
    if(actions[*it]!=Traversal::walk){result.points.push_back(landings[*it]);result.traversal.push_back(actions[*it]);}
   }
   result.points.push_back(to->point);
   result.traversal.push_back(Traversal::walk);
   std::vector<Triangle> corridor{triangles[from->triangle]};
   for(auto index:chain)corridor.push_back(triangles[index]);
   smooth(result,corridor);return result;
  }
  const auto& tri=triangles[current.index];
  auto relax=[&](std::size_t next,Vec midpoint,Vec landing,Traversal action){
   float actionCost=action==Traversal::walk?0.f:250.f+std::abs(landing.z-midpoint.z)*.5f;
   auto cost=current.cost+(entries[current.index]-midpoint).length()+(landing-midpoint).length()+actionCost;
   if(cost>=costs[next])return;
   costs[next]=cost;parent[next]=current.index;portals[next]=midpoint;
   landings[next]=landing;actions[next]=action;
   entries[next]=landing;
   queue.push({cost+(landing-to->point).length(),cost,next});
  };
  for(std::size_t edge=0;edge<3;++edge){
   auto found=indices.find(tri.neighbors[edge]);if(found==indices.end())continue;
   auto next=found->second;const auto& neighbor=triangles[next];if(!walkable(neighbor))continue;
   auto midpoint=(tri.vertices[edge]+tri.vertices[(edge+1)%3])*.5f;
   // Never connect corrupt portal indices, disconnected floors or a gap in the mesh.
   auto landing=closest(midpoint,neighbor);auto action=tri.traversal[edge];
   if(action==Traversal::walk){if((landing-midpoint).length()>4.f)continue;}
   else{
    auto endpoints=traversalEndpoints(tri,edge,neighbor);midpoint=endpoints.first;landing=endpoints.second;
    if(!traversable(midpoint,landing,action))continue;
   }
   relax(next,midpoint,landing,action);
  }
  for(const auto& link:tri.links){
   auto found=indices.find(link.target);if(found==indices.end())continue;
   if(!traversable(link.takeoff,link.landing,link.type,link.dropLimit))continue;
   if((closest(link.takeoff,tri)-link.takeoff).length()>4.f||(closest(link.landing,triangles[found->second])-link.landing).length()>4.f)continue;
   relax(found->second,link.takeoff,link.landing,link.type);
  }
 }
 return result;
}
}
