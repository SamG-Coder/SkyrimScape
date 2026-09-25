#include "navigation.hpp"
#include "terrain_overlay.hpp"
#include "terrain_links.hpp"
#include <cstdlib>
#include <iostream>
using namespace scape;
void require(bool ok,const char* reason){if(!ok){std::cerr<<reason<<'\n';std::exit(1);}}
bool equal(Vec a,Vec b){return (a-b).length()<.01f;}
int main(){
 std::vector<nav::Triangle> mesh;
 // Two sides of a wall with a passage around its far end.
 for(int x=0;x<5;++x)for(int y=0;y<5;++y){
  if(x==2&&y<4)continue;
  Vec a{100.f*x,100.f*y,0},b{a.x+100,a.y,0},c{a.x+100,a.y+100,0},d{a.x,a.y+100,0};
  mesh.push_back({0x12340000+mesh.size(),{a,b,c}});
  mesh.push_back({0x12340000+mesh.size(),{a,c,d}});
 }
 for(auto& a:mesh)for(auto& b:mesh)if(a.key!=b.key)for(int e=0;e<3;++e)for(int f=0;f<3;++f){
  if(equal(a.vertices[e],b.vertices[(f+1)%3])&&equal(a.vertices[(e+1)%3],b.vertices[f]))a.neighbors[e]=b.key;
 }
 auto result=nav::plan(mesh,{50,50,0},{450,50,0},10);
 require(!result.points.empty(),"A connected route around the wall must exist");
 bool detoured=false;
 for(auto p:result.points)if(p.y>=400)detoured=true;
 require(detoured,"The route must go around the wall, not cut across it");
 for(std::size_t i=1;i<result.points.size();++i)for(int sample=0;sample<=20;++sample){
  auto p=result.points[i-1]+(result.points[i]-result.points[i-1])*(sample/20.f);
  require(nav::locate(mesh,p,.1f).has_value(),"Every route segment must stay on walkable triangles");
 }
 auto disconnected=mesh;for(auto& t:disconnected)t.neighbors.fill(nav::noKey);
 require(nav::plan(disconnected,{50,20,0},{450,20,0},10).points.empty(),"Disconnected areas must not receive a fake direct route");
 require(nav::plan(mesh,{50,20,0},{450,20,500},10).points.empty(),"An upstairs destination must not snap to the lower floor");
 auto same=nav::plan(mesh,{10,5,0},{20,10,0},10);
 require(same.points.size()==2,"A single-triangle route must be direct");
 nav::Triangle degenerate{1,{{{0,0,0},{0,0,0},{0,0,0}}}};
 require(!nav::walkable(degenerate),"Zero-area triangles must be excluded");
 nav::Triangle wall{2,{{{0,0,0},{0,100,0},{0,0,100}}}};
 require(!nav::walkable(wall),"Vertical walls must not be walkable");
 // Corrupt adjacency claims that two distant triangles share an edge.
 auto bad=disconnected;bad.front().neighbors[0]=bad.back().key;
 require(nav::plan(bad,{50,20,0},{450,480,0},10).points.empty(),"Invalid portals must not bridge physical gaps");
 std::vector<nav::Triangle> ledges{
  {1,{{{0,0,0},{100,0,0},{0,-100,0}}}},
  {2,{{{0,50,50},{0,150,50},{100,50,50}}}}
 };
 ledges[0].neighbors[0]=2;ledges[0].traversal[0]=nav::Traversal::jump;
 ledges[1].neighbors[2]=1;ledges[1].traversal[2]=nav::Traversal::drop;
 auto jump=nav::plan(ledges,{20,-20,0},{20,70,50},10);
 require(!jump.points.empty(),"A bounded authored jump-up link must form a route");
 auto jumpStep=std::find(jump.traversal.begin(),jump.traversal.end(),nav::Traversal::jump);
 require(jumpStep!=jump.traversal.end(),"Smoothing must preserve the jump transition");
 auto jumpIndex=static_cast<std::size_t>(jumpStep-jump.traversal.begin());
 require(jumpIndex>0&&nav::traversable(jump.points[jumpIndex-1],jump.points[jumpIndex],nav::Traversal::jump),"Takeoff and landing must survive smoothing");
 auto drop=nav::plan(ledges,{20,70,50},{20,-20,0},10);
 require(std::find(drop.traversal.begin(),drop.traversal.end(),nav::Traversal::drop)!=drop.traversal.end(),"A bounded drop must retain its directed action");
 auto tooHigh=ledges;for(auto& p:tooHigh[1].vertices)p.z=250;
 require(nav::plan(tooHigh,{20,-20,0},{20,70,250},10).points.empty(),"An unreachable jump height must reject the link");
 require(nav::plan(tooHigh,{20,70,250},{20,-20,0},10).points.empty(),"A deep drop must reject the link");
 auto projected=nav::surface(ledges,{20,70,400});
 require(projected&&std::abs(projected->point.z-50)<.01f,"Elevated click must project to the walkable surface below it");
 require(!nav::surface(ledges,{400,400,50}),"Surface normalization must not drag a distant click sideways");
 require(!nav::surface(ledges,{20,70,-200}),"Surface normalization must not jump to a floor overhead");
 auto colors=nav::reachability(ledges,{20,-20,0});
 require(colors[0]==1&&colors[1]==2,"Overlay must distinguish walking from directed jump reachability");
 colors=nav::reachability(disconnected,{50,20,0});
 require(colors.front()==1&&colors.back()==0,"Disconnected overlay terrain must not be marked reachable");
 colors=nav::reachability(bad,{50,20,0});
 require(colors.back()==0,"Overlay must reject corrupt portals just like the planner");
 auto clipped=nav::clipScreen({{-1,.5f},{.5f,-1},{2,2}});
 require(clipped.size()>=3,"A triangle crossing the viewport must remain drawable");
 for(auto p:clipped)require(p.x>=0&&p.x<=1&&p.y>=0&&p.y<=1,"Overlay clipping must remain within the viewport");
 require(nav::clipScreen({{-3,0},{-2,0},{-2,1}}).empty(),"Off-screen triangles must be discarded");
 std::vector<nav::Triangle> unmarked{
  {10,{{{0,0,100},{600,0,100},{0,-600,100}}}},
  {20,{{{440,40,0},{480,40,0},{460,100,0}}}}
 };
 auto original=unmarked;
 auto count=nav::addTerrainLinks(unmarked,{300,0,100},700.f,180.f,[](Vec,Vec,nav::Traversal){return true;});
 require(count>0,"Fine edge samples must discover an unmarked drop away from a long edge midpoint");
 require(unmarked[1].links.empty(),"A walk-off connection must not invent an uphill return route");
 auto calculated=nav::plan(unmarked,{440,-40,100},{460,60,0},10);
 require(!calculated.points.empty()&&std::find(calculated.traversal.begin(),calculated.traversal.end(),nav::Traversal::drop)!=calculated.traversal.end(),"Calculated drops must be usable by actual A-star routing");
 require(nav::plan(unmarked,{460,60,0},{440,-40,100},10).points.empty(),"A lower landing must not traverse a drop backwards");
 colors=nav::reachability(unmarked,{440,-40,100});require(colors[1]==2,"Overlay must include calculated drop connectivity");
 auto blocked=original;require(nav::addTerrainLinks(blocked,{300,0,100},700.f,180.f,[](Vec,Vec,nav::Traversal){return false;})==0,"Blocked landing clearance must prevent generated connections");
 auto deep=original;for(auto& p:deep[0].vertices)p.z=400;
 require(nav::addTerrainLinks(deep,{300,0,400},700.f,180.f,[](Vec,Vec,nav::Traversal){return true;})==0,"Drops beyond the allowed threshold must not connect");
 std::vector<nav::Triangle> broad{
  {100,{{{0,0,0},{2000,0,0},{2000,2000,0}}}},
  {101,{{{0,0,0},{2000,2000,0},{0,2000,0}}}}
 };
 broad[0].neighbors[2]=101;broad[1].neighbors[0]=100;
 Vec nearA{1950,50,0},nearB{50,1950,0};auto through=nav::plan(broad,nearA,nearB,10);
 float routeLength=0;for(std::size_t i=1;i<through.points.size();++i)routeLength+=(through.points[i]-through.points[i-1]).length();
 require(!through.points.empty()&&routeLength<=(nearB-nearA).length()+1.f,"Broad open triangles must not force centroid detours beyond the smoothing distance");
 std::cout<<"PASS: wall detour, segment containment, disconnected floors, endpoint projection, invalid portals and degenerate triangles\n";
}
