#include "control_math.hpp"
#include <cstdlib>
#include <iostream>
void require(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
float dot(scape::Vec a,scape::Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
int main(){
 for(float yaw:{-3.1f,-1.57f,0.f,1.57f,3.1f})for(float pitch:{.2f,.72f,1.35f}){
  scape::Orbit orbit{yaw,pitch,650.f};auto r=orbit.right(),f=orbit.forward(),u=orbit.up();
  require(std::abs(r.length()-1)<1e-5f&&std::abs(f.length()-1)<1e-5f&&std::abs(u.length()-1)<1e-5f,"Camera axes must remain unit length");
  require(std::abs(dot(r,f))<1e-5f&&std::abs(dot(r,u))<1e-5f&&std::abs(dot(f,u))<1e-5f,"Orbit axes must remain perpendicular");
  scape::Vec focus{123,-456,89};auto offset=focus-orbit.position(focus);
  require(std::abs(offset.length()-650)<.001f&&dot(offset,f)>649.99f,"Camera must face the focus and preserve requested distance");
 }
 scape::Orbit orbit;orbit.rotate(1e5f,1e5f);orbit.zoom(1000);
 require(orbit.pitch==1.35f&&orbit.distance==90.f,"Orbit limits must prevent inversion and zero distance");
 orbit.rotate(0,-1e5f);orbit.zoom(-1000);
 require(orbit.pitch==.2f&&orbit.distance==4000.f,"Far zoom and low pitch must be bounded");
 require(!scape::reached({0,0,0},{0,0,400},130),"A target upstairs must not activate through the ceiling");
 require(scape::reached({0,0,0},{10,10,1},24),"Nearby ground destination must complete");
 require(scape::passedWaypoint({115,8,0},{0,0,0},{100,0,0}),"Small waypoint overshoot must advance instead of reversing");
 require(!scape::passedWaypoint({90,0,0},{0,0,0},{100,0,0}),"A waypoint ahead must not be skipped");
 require(!scape::passedWaypoint({115,80,0},{0,0,0},{100,0,0}),"Passing a plane far beside a corner must not skip it");
 require(std::abs(scape::heading({0,0,0},{10,0,0})-1.5707963f)<1e-5f,"Eastward order must face east");
 require(std::abs(scape::heading({0,0,0},{0,10,0}))<1e-5f,"Northward order must face north");
 for(float camera:{0.f,1.57f,3.14159265f,-1.57f})for(float desired:{0.f,1.57f,3.14159265f}){
  auto input=scape::cameraRelativeInput(desired,camera);
  float x=std::cos(camera)*input.x+std::sin(camera)*input.y;
  float y=-std::sin(camera)*input.x+std::cos(camera)*input.y;
  require(std::abs(x-std::sin(desired))<1e-5f&&std::abs(y-std::cos(desired))<1e-5f,"Orbit rotation must not reverse the world movement direction");
 }
 require(scape::wheelStep(9,1)==-1&&scape::wheelStep(8,1)==1,"Wheel down must zoom out, wheel up must zoom in");
 require(scape::wheelStep(9,0)==0&&scape::wheelStep(0,1)==0,"Wheel release and regular clicks must not zoom");
 auto initialDistance=orbit.distance;orbit.zoom(scape::wheelStep(8,1));require(orbit.distance<initialDistance,"Wheel up must reduce camera distance");
 initialDistance=orbit.distance;orbit.zoom(scape::wheelStep(9,1));require(orbit.distance>initialDistance,"Wheel down must increase camera distance");
 auto initial=scape::clickAnimation(0),middle=scape::clickAnimation(.3f),end=scape::clickAnimation(.6f);
 require(scape::jumpLanding({0,0,0},{120,0,40}),"Short low obstacle must allow a bounded jump landing");
 require(!scape::jumpLanding({0,0,0},{300,0,0})&&!scape::jumpLanding({0,0,0},{120,0,120})&&!scape::jumpLanding({0,0,0},{120,0,-300}),"Jump planning must reject long gaps, high ledges and cliffs");
 require(initial.visible&&middle.visible&&!end.visible,"Click pulse must expire after 600ms");
 require(middle.radius>initial.radius&&middle.alpha<initial.alpha,"Click pulse must expand and fade");
 std::cout<<"PASS: orbit basis, focus, pitch/zoom bounds, heading and vertical arrival checks\n";
}
