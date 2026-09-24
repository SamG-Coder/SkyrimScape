#pragma once
#include "navigation.hpp"
#include "route_recorder.hpp"
#include "terrain_links.hpp"
#include <functional>
namespace native_navigation {
inline scape::nav::Key key(RE::FormID mesh,std::uint16_t triangle){return (static_cast<std::uint64_t>(mesh)<<16)|triangle;}
inline std::vector<scape::nav::Triangle> snapshot(){
 std::vector<scape::nav::Triangle> result;
 auto tes=RE::TES::GetSingleton();if(!tes)return result;
 tes->ForEachCell([&](RE::TESObjectCELL* cell){
  if(result.size()>=50000)return;
  if(!cell||!cell->IsAttached())return;
  auto array=cell->GetRuntimeData().navMeshes;if(!array)return;
  for(const auto& mesh:array->navMeshes){
   if(!mesh||mesh->triangles.size()>65535)continue;
   for(std::size_t i=0;i<mesh->triangles.size();++i){
    auto& source=mesh->triangles[i];
    if(source.triangleFlags.any(RE::BSNavmeshTriangle::TriangleFlag::kDeleted))continue;
    scape::nav::Triangle triangle;triangle.key=key(mesh->GetFormID(),static_cast<std::uint16_t>(i));bool valid=true;
    for(std::size_t e=0;e<3;++e){
     if(source.vertices[e]>=mesh->vertices.size()){valid=false;break;}
     auto p=mesh->vertices[source.vertices[e]].location;triangle.vertices[e]={p.x,p.y,p.z};
     const auto adjacent=source.triangles[e];
     if(static_cast<std::uint16_t>(*source.triangleFlags)&(1u<<e)){
      if(adjacent>=mesh->extraEdgeInfo.size())continue;
      auto& extra=mesh->extraEdgeInfo[adjacent];
      if(extra.type==RE::EDGE_EXTRA_INFO_TYPE::kPortal||extra.type==RE::EDGE_EXTRA_INFO_TYPE::kLedgeUp||extra.type==RE::EDGE_EXTRA_INFO_TYPE::kLedgeDown){
       triangle.neighbors[e]=key(extra.portal.otherMeshID,extra.portal.triangle);
       if(extra.type==RE::EDGE_EXTRA_INFO_TYPE::kLedgeUp)triangle.traversal[e]=scape::nav::Traversal::jump;
       if(extra.type==RE::EDGE_EXTRA_INFO_TYPE::kLedgeDown)triangle.traversal[e]=scape::nav::Traversal::drop;
      }
     }else if(adjacent<mesh->triangles.size())triangle.neighbors[e]=key(mesh->GetFormID(),adjacent);
    }
    if(valid&&scape::nav::walkable(triangle))result.push_back(triangle);
    if(result.size()>=50000)return;
   }
  }
 });
 return result;
}
inline std::vector<scape::nav::Triangle> validatedSnapshot(const std::function<bool(scape::Vec,scape::Vec,scape::nav::Traversal)>& clearTraversal){
 auto mesh=snapshot();
 std::unordered_map<scape::nav::Key,std::size_t> indices;
 for(std::size_t i=0;i<mesh.size();++i)indices.emplace(mesh[i].key,i);
 std::size_t links=0;
 for(auto& triangle:mesh)for(std::size_t edge=0;edge<3;++edge){
  if(triangle.traversal[edge]==scape::nav::Traversal::walk)continue;
  auto found=indices.find(triangle.neighbors[edge]);if(found==indices.end())continue;
  auto [takeoff,landing]=scape::nav::traversalEndpoints(triangle,edge,mesh[found->second]);
  if(!scape::nav::traversable(takeoff,landing,triangle.traversal[edge])||!clearTraversal(takeoff,landing,triangle.traversal[edge]))triangle.neighbors[edge]=scape::nav::noKey;
  else ++links;
 }
 auto player=RE::PlayerCharacter::GetSingleton();auto position=player->GetPosition();float maxDrop=180.f;
 if(auto settings=RE::GameSettingCollection::GetSingleton())if(auto setting=settings->GetSetting("fJumpFallHeightMin")){
  auto threshold=setting->GetFloat();if(std::isfinite(threshold)&&threshold>=40.f)maxDrop=(std::min)(512.f,threshold*.75f);
 }
 const auto generated=scape::nav::addTerrainLinks(mesh,{position.x,position.y,position.z},2200.f,maxDrop,clearTraversal);
 spdlog::info("Terrain connections: {} native, {} calculated drops, drop limit {:.1f}",links,generated,maxDrop);
 return mesh;
}
inline scape::nav::Route plan(scape::Vec start,scape::Vec finish,bool normalizeGround,
 const std::function<bool(scape::Vec,scape::Vec,scape::nav::Traversal)>& clearTraversal){
 auto mesh=validatedSnapshot(clearTraversal);
 if(normalizeGround)if(auto floor=scape::nav::surface(mesh,finish)){
  if((floor->point-finish).length()>40.f)spdlog::info("Ground click projected onto surface: height {:.1f} -> {:.1f}",finish.z,floor->point.z);
  finish=floor->point;
 }
 auto route=scape::nav::plan(mesh,start,finish);
 if(route.points.empty()){
  auto from=scape::nav::locate(mesh,start,160.f),to=scape::nav::locate(mesh,finish,160.f);
  spdlog::info("Route rejected: {} triangles, start projected {}, goal projected {}, expanded {}; start {:.1f} {:.1f} {:.1f}, goal {:.1f} {:.1f} {:.1f}",mesh.size(),bool(from),bool(to),route.expanded,start.x,start.y,start.z,finish.x,finish.y,finish.z);
 }
 // Bounded latest accepted/rejected snapshots; slow formatting and disk IO stay off input.
 if(auto directory=SKSE::log::log_directory()){
  static scape::nav::RouteRecorder recorder;
  recorder.record(*directory,start,finish,std::move(mesh),route);
 }
 return route;
}
}
