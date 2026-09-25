#include "grid_navigation.hpp"
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
 auto shortEdges=[](Vec a,Vec b,nav::Traversal){return (b-a).length()<60.f;};
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
 std::cout<<"PASS: grid routing, any-angle smoothing, group caching, circular clearance, directed drops and height layers\n";
}
