#include "grid_navigation.hpp"
#include "route_following.hpp"
#include <cstdlib>
#include <iostream>
using namespace scape;
void require(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
void rectangle(std::vector<nav::Triangle>& mesh,float x,float y,float w,float h,float z){
 nav::Key key=mesh.size()+1;mesh.push_back({key,{{{x,y,z},{x+w,y,z},{x+w,y+h,z}}}});mesh.push_back({key+1,{{{x,y,z},{x+w,y+h,z},{x,y+h,z}}}});
}
int main(){
 auto clear=[](Vec,Vec,nav::Traversal){return true;};
 std::vector<nav::Triangle> floor;rectangle(floor,0,0,512,512,0);
 grid::World world;world.update(floor,{256,256,0},600);
 auto route=world.plan({64,64,0},{448,448,0},clear,180);
 require(!route.route.points.empty(),"Grid must route over a continuous floor without triangle adjacency links");
 require(route.route.points.size()<=4,"Open grid route must simplify to smooth any-angle travel");
 auto forward=world.plan({67,80,0},{448,80,0},clear,180);
 require(forward.route.points.size()>1&&forward.route.points[1].x>67,"Open-floor first leg must not step backward to the nearest cell");
 auto actorBlocks=[](Vec a,Vec b,nav::Traversal){return planarDistance(a,{400,240,0})>30&&planarDistance(b,{400,240,0})>30;};
 auto approach=world.plan({64,240,0},{400,240,0},actorBlocks,180,80);
 require(!approach.route.points.empty(),"Occupied actor center must not prevent an approach route");
 require(planarDistance(approach.route.points.back(),{400,240,0})<=80&&planarDistance(approach.route.points.back(),{400,240,0})>30,"Approach must stop within reach outside actor collider");
 auto visibleApproach=world.plan({64,240,0},{400,240,0},actorBlocks,180,80,24000,(std::chrono::milliseconds::max)(),[](Vec p){return p.y>280;});
 require(!visibleApproach.route.points.empty()&&visibleApproach.route.points.back().y>280,"Approach must search past cells without target visibility");
 auto shortEdges=[](Vec a,Vec b,nav::Traversal){return !((std::min)(a.x,b.x)<256.f&&(std::max)(a.x,b.x)>192.f&&(std::min)(a.y,b.y)<384.f);};
 auto sliced=world.plan({64,64,0},{448,448,0},shortEdges,180,0,1);
 require(sliced.pending&&sliced.stats.expanded==1,"Search must yield at the per-frame expansion budget");
 std::size_t frames=1,expanded=sliced.stats.expanded;
 while(sliced.pending&&frames<1000){
  sliced=world.plan({64,64,0},{448,448,0},shortEdges,180,0,1);++frames;
  require(sliced.stats.expanded>=expanded,"Continuation must preserve search progress");expanded=sliced.stats.expanded;
 }
 require(!sliced.pending&&!sliced.route.points.empty()&&frames>1,"Sliced search must eventually return a complete route");
 world.plan({64,64,0},{448,448,0},shortEdges,180,0,1);
 auto retarget=world.plan({64,64,0},{96,64,0},clear,180);
 require(!retarget.route.points.empty()&&planarDistance(retarget.route.points.back(),{96,64,0})<1,"New destination must replace pending search");
 auto blockedNow=world.plan({64,64,0},{448,448,0},[](Vec a,Vec b,nav::Traversal){return a.x<100&&b.x<100;},180);
 require(blockedNow.route.points.empty(),"A new order must not reuse stale dynamic collision acceptance");
 world.update(floor,{256,256,0},600);
 require(world.statistics().reused>0&&world.statistics().built==0,"Unchanged tile groups must be reused");
 require(world.canWalk({64,64,0},{448,448,0},clear),"Unchanged cache must retain usable radius clearance");
 require(sameWalkDirection({0,0,0},{100,0,0},{200,30,0})&&!sameWalkDirection({0,0,0},{100,0,0},{-100,0,0}),"Only compatible forward clicks may preserve current movement");
 nav::Route moving;moving.points={{64,64,0},{128,64,0},{256,64,0}};moving.traversal.assign(3,nav::Traversal::walk);
 require(joinMovingRoute(moving,{144,64,0},[&](Vec a,Vec b){return world.canWalk(a,b,clear);})&&moving.points.size()==2&&moving.points.back().x==256,"Moving plan completion must join ahead without returning to origin");
 nav::Route jumping;jumping.points={{64,64,0},{128,64,0},{192,64,0}};jumping.traversal={nav::Traversal::walk,nav::Traversal::walk,nav::Traversal::jump};
 require(joinMovingRoute(jumping,{80,64,0},[](Vec,Vec){return true;})&&jumping.traversal.back()==nav::Traversal::jump&&jumping.points[1].x==128,"Moving join must retain the traversal takeoff");
 auto blockedJoin=moving;require(!joinMovingRoute(blockedJoin,{144,64,0},[](Vec,Vec){return false;}),"Blocked joining segment must reject route replacement");
 floor[0].vertices[0].z=4;world.update(floor,{256,256,0},600);
 require(world.statistics().built>0,"Changed geometry must invalidate affected tile groups");
 std::vector<nav::Triangle> narrow;rectangle(narrow,0,0,512,24,0);world.update(narrow,{256,12,0},600);
 require(world.plan({64,12,0},{448,12,0},clear,180).route.points.empty(),"Circular footprint must reject a corridor narrower than the character");
 std::vector<nav::Triangle> levels;rectangle(levels,0,0,256,256,100);rectangle(levels,288,0,256,256,0);
 world.update(levels,{256,128,100},600);
 auto drop=world.plan({128,128,100},{416,128,0},clear,180);
 require(!drop.route.points.empty(),"Grid must calculate a drop across an unlinked terrain boundary");
 require(std::find(drop.route.traversal.begin(),drop.route.traversal.end(),nav::Traversal::drop)!=drop.route.traversal.end(),"Drop must remain explicit after smoothing");
 require(world.plan({416,128,0},{128,128,100},clear,180).route.points.empty(),"Grid must not reverse a drop into an unreachable jump");
 world.update(levels,{256,128,100},600);
 auto blocked=world.plan({128,128,100},{416,128,0},[](Vec,Vec,nav::Traversal t){return t==nav::Traversal::walk;},180);
 require(blocked.route.points.empty(),"Physics rejection must prevent a proposed traversal");
 std::vector<nav::Triangle> stacked;rectangle(stacked,0,0,256,256,0);rectangle(stacked,0,0,256,256,300);world.update(stacked,{128,128,0},600);
 require(world.plan({64,64,0},{192,192,300},clear,180).route.points.empty(),"Height layers must not connect through a ceiling");
 std::vector<nav::Triangle> stairs;
 for(int i=0;i<10;++i)rectangle(stairs,i*48.f,0,48,256,i*16.f);
 world.update(stairs,{240,128,64},600);
 auto steps=world.plan({64,128,16},{416,128,128},clear,180);
 require(!steps.route.points.empty(),"Small staircase risers must form a walking route");
 require(std::all_of(steps.route.traversal.begin(),steps.route.traversal.end(),[](auto t){return t==nav::Traversal::walk;}),"Steps within the walking limit must not generate jumps");
 require(world.canWalk({416,128,128},{64,128,16},clear),"Small stairs must also be walkable downhill");
 std::vector<nav::Triangle> tall;rectangle(tall,0,0,256,256,0);rectangle(tall,256,0,256,256,48);world.update(tall,{256,128,0},600);
 require(!world.canWalk({128,128,0},{384,128,48},clear),"A tall riser must not be smoothed into a walk");
 std::vector<nav::Triangle> ramp;rectangle(ramp,0,0,512,256,0);
 for(auto& t:ramp)for(auto& v:t.vertices)v.z=v.x*.8f;
 world.update(ramp,{256,128,200},600);
 require(world.canWalk({64,128,51.2f},{448,128,358.4f},clear),"Walkable ramps must follow their floor heights");
 for(auto& t:ramp)for(auto& v:t.vertices)v.z=v.x*1.5f;
 world.update(ramp,{256,128,300},600);
 require(!world.canWalk({64,128,96},{448,128,672},clear),"Slopes above 45 degrees must not be treated as ramps");
 std::vector<nav::Triangle> crest;rectangle(crest,0,0,256,256,0);rectangle(crest,256,0,256,256,0);
 for(auto& t:crest)for(auto& v:t.vertices)v.z=(v.x<=256?v.x:512-v.x)*.5f;
 world.update(crest,{256,128,128},600);
 require(world.canWalk({64,128,32},{448,128,32},clear),"A walkable crest must sample the ground rather than a line through the hill");
 std::cout<<"PASS: grid routing, ramps, stairs, slope limits, caching, clearance, directed drops and height layers\n";
}
