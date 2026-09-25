#include "navigation.hpp"
#include "terrain_links.hpp"
#include "grid_navigation.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
int main(int argc,char** argv){
 if(argc!=2&&argc!=3){std::cerr<<"Usage: SkyrimScapeRouteReplay snapshot.csv [--terrain]\n";return 2;}
 std::ifstream input(argv[1]);if(!input){std::cerr<<"Cannot open snapshot\n";return 2;}
 scape::Vec start{},finish{};std::vector<scape::nav::Triangle> mesh;std::vector<scape::Vec> recorded;bool request=false;
 std::string line;
 while(std::getline(input,line)){
  std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);std::string kind;row>>kind;
  if(kind=="request"){row>>start.x>>start.y>>start.z>>finish.x>>finish.y>>finish.z;request=true;}
  else if(kind=="triangle"){
   scape::nav::Triangle t;row>>t.key;for(auto& p:t.vertices)row>>p.x>>p.y>>p.z;for(auto& key:t.neighbors)row>>key;
   if(!row){std::cerr<<"Incomplete triangle\n";return 2;}
   row>>std::ws;if(!row.eof())for(auto& type:t.traversal){int value;row>>value;if(value<0||value>2)return 2;type=static_cast<scape::nav::Traversal>(value);}
   row.clear();mesh.push_back(t);
  }else if(kind=="link"){
   scape::nav::Key source;scape::nav::TerrainLink link;int type{};
   row>>source>>link.target>>link.takeoff.x>>link.takeoff.y>>link.takeoff.z>>link.landing.x>>link.landing.y>>link.landing.z>>type>>link.dropLimit;
   if(mesh.empty()||mesh.back().key!=source||type<1||type>2)return 2;
   link.type=static_cast<scape::nav::Traversal>(type);mesh.back().links.push_back(link);
  }else if(kind=="waypoint"){scape::Vec p;row>>p.x>>p.y>>p.z;recorded.push_back(p);}
  else{std::cerr<<"Unknown record\n";return 2;}
  if(!row){std::cerr<<"Incomplete record\n";return 2;}
 }
 if(!request||mesh.empty()){std::cerr<<"Missing request or mesh\n";return 2;}
 if(argc==3){
  if(std::string(argv[2])=="--grid"){
   auto began=std::chrono::steady_clock::now();scape::grid::World grid;grid.update(mesh,start);
   auto updated=std::chrono::steady_clock::now();auto result=grid.plan(start,finish,[](scape::Vec,scape::Vec,scape::nav::Traversal){return true;},450.f);
   auto planned=std::chrono::steady_clock::now();grid.update(mesh,start);auto reused=std::chrono::steady_clock::now();
   std::cout<<"GRID geometry only: "<<result.reason<<", "<<result.stats.cells<<" cells, "<<result.stats.groups<<" groups, "<<result.stats.expanded<<" expanded, "<<result.route.points.size()<<" points; build "<<std::chrono::duration<double,std::milli>(updated-began).count()<<"ms, plan "<<std::chrono::duration<double,std::milli>(planned-updated).count()<<"ms, cached refresh "<<std::chrono::duration<double,std::milli>(reused-planned).count()<<"ms, "<<grid.statistics().reused<<" reused groups\n";return 0;
  }
  if(std::string(argv[2])!="--terrain")return 2;
  auto began=std::chrono::steady_clock::now();std::size_t probes=0;
  auto generated=scape::nav::addTerrainLinks(mesh,start,2200.f,450.f,[&](scape::Vec,scape::Vec,scape::nav::Traversal){++probes;return true;});
  std::cout<<"Geometric candidates (physics not evaluated): "<<generated<<" links, "<<probes<<" validations, "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count()<<" ms\n";return 0;
 }
 auto route=scape::nav::plan(mesh,start,finish);
 std::cout<<mesh.size()<<" triangles; "<<route.expanded<<" expanded; "<<route.points.size()<<" waypoints\n";
 for(auto pair:{std::pair{"start",start},std::pair{"goal",finish}}){
  if(auto nearest=scape::nav::locate(mesh,pair.second,100000.f)){
   auto p=nearest->point;std::cout<<pair.first<<" nearest distance "<<(p-pair.second).length()<<" at "<<p.x<<' '<<p.y<<' '<<p.z<<'\n';
  }
 }
 if(recorded.size()!=route.points.size()){std::cerr<<"Replay waypoint count differs\n";return 1;}
 for(std::size_t i=0;i<recorded.size();++i)if((recorded[i]-route.points[i]).length()>.05f){std::cerr<<"Replay waypoint differs\n";return 1;}
 for(std::size_t i=1;i<route.points.size();++i)for(int sample=0;sample<=32;++sample){
  if(route.traversal[i]!=scape::nav::Traversal::walk)continue;
  auto p=route.points[i-1]+(route.points[i]-route.points[i-1])*(sample/32.f);
  if(!scape::nav::locate(mesh,p,8.f)){std::cerr<<"Route leaves mesh at leg "<<i<<'\n';return 1;}
 }
 std::cout<<"PASS: reproduced captured outcome and checked route containment\n";
}
