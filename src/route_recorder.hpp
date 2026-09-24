#pragma once
#include "navigation.hpp"
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <thread>
namespace scape::nav {
// The worker owns copies of plain data only; it never calls into Skyrim.
class RouteRecorder {
 struct Job {std::filesystem::path path;Vec start,finish;std::vector<Triangle> mesh;std::vector<Vec> points;};
 std::mutex mutex;
 std::condition_variable_any ready;
 std::array<std::optional<Job>,2> pending;
 std::jthread worker;
 void run(std::stop_token stop){
  while(!stop.stop_requested()){
   std::optional<Job> job;
   {
    std::unique_lock lock(mutex);
    if(!ready.wait(lock,stop,[&]{return pending[0]||pending[1];}))return;
    auto index=pending[0]?0:1;job=std::move(pending[index]);pending[index].reset();
   }
   const auto& j=*job;
   std::ofstream output(j.path);
   output<<std::setprecision(9)<<"request,"<<j.start.x<<','<<j.start.y<<','<<j.start.z<<','<<j.finish.x<<','<<j.finish.y<<','<<j.finish.z<<'\n';
   for(const auto& triangle:j.mesh){
    output<<"triangle,"<<triangle.key;
    for(auto p:triangle.vertices)output<<','<<p.x<<','<<p.y<<','<<p.z;
    for(auto neighbor:triangle.neighbors)output<<','<<neighbor;
    for(auto type:triangle.traversal)output<<','<<static_cast<int>(type);
    output<<'\n';
    for(auto& link:triangle.links)output<<"link,"<<triangle.key<<','<<link.target<<','<<link.takeoff.x<<','<<link.takeoff.y<<','<<link.takeoff.z<<','<<link.landing.x<<','<<link.landing.y<<','<<link.landing.z<<','<<static_cast<int>(link.type)<<','<<link.dropLimit<<'\n';
   }
   for(auto p:j.points)output<<"waypoint,"<<p.x<<','<<p.y<<','<<p.z<<'\n';
  }
 }
public:
 RouteRecorder():worker([this](std::stop_token stop){run(stop);}){}
 void record(const std::filesystem::path& directory,Vec start,Vec finish,std::vector<Triangle> mesh,const Route& route){
  const auto index=route.points.empty()?1:0;
  Job job{directory/(index?"SkyrimScape-route-rejected.csv":"SkyrimScape-route-accepted.csv"),start,finish,std::move(mesh),route.points};
  {std::lock_guard lock(mutex);pending[index]=std::move(job);}
  ready.notify_one();
 }
};
}
