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
