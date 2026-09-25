#pragma once
#include "terrain_links.hpp"
#include <bit>
#include <functional>
#include <unordered_set>
#include <chrono>
namespace scape::grid {
constexpr float spacing=32.f,radius=20.f;
constexpr int groupSize=16;
struct Cell {int x{},y{};Vec position;};
struct Group {std::uint64_t signature{};std::vector<Cell> cells;};
struct Stats {std::size_t groups{},reused{},built{},cells{},expanded{},clearanceChecks{};};
struct Result {nav::Route route;Stats stats;const char* reason{"no-route"};bool pending{};};
inline int coordinate(float v){return static_cast<int>(std::floor(v/spacing));}
inline int groupCoordinate(int v){return static_cast<int>(std::floor(v/static_cast<float>(groupSize)));}
inline std::uint64_t key(int x,int y){return nav::gridKey(x,y);}
class World {
 std::vector<nav::Triangle> mesh;
 std::unordered_map<std::uint64_t,std::vector<std::size_t>> sources;
 std::unordered_map<std::uint64_t,Group> groups;
 std::vector<Cell> cells;
 std::unordered_map<std::uint64_t,std::vector<std::size_t>> columns;
 std::vector<signed char> clearance;
 std::unordered_map<std::uint64_t,bool> edges;
 Stats stats;
 struct Entry {float f,g;std::size_t i;bool operator<(const Entry& b)const{return f>b.f;}};
 struct Search {
  Vec start,finish;float goalRadius{};std::size_t from{},to{},expanded{},checks{};
  std::priority_queue<Entry> queue;std::vector<float> cost;
  std::vector<std::size_t> parent;std::vector<nav::Traversal> actions;
 };
 std::optional<Search> search;
 bool supportAt(float x,float y,float z)const{
  auto found=sources.find(key(groupCoordinate(coordinate(x)),groupCoordinate(coordinate(y))));if(found==sources.end())return false;
  for(auto index:found->second){
   const auto& t=mesh[index];auto p=nav::verticalPoint({x,y,z},t);
   if(std::abs(p.z-z)<=32.f&&(nav::closest(p,t)-p).length()<1.f)return true;
  }
  return false;
 }
 bool clearCell(std::size_t i){
  if(clearance[i]>=0)return clearance[i]!=0;
  auto p=cells[i].position;
  if(!footprint(p)){clearance[i]=0;return false;}
  clearance[i]=1;return true;
 }
 std::optional<std::size_t> nearest(Vec p,float maxHorizontal,float maxVertical){
  std::optional<std::size_t> best;float score=std::numeric_limits<float>::infinity();
  int reach=static_cast<int>(std::ceil(maxHorizontal/spacing));auto x=coordinate(p.x),y=coordinate(p.y);
  for(int dx=-reach;dx<=reach;++dx)for(int dy=-reach;dy<=reach;++dy){
   auto found=columns.find(key(x+dx,y+dy));if(found==columns.end())continue;
   for(auto i:found->second){auto q=cells[i].position;float h=planarDistance(p,q),v=std::abs(p.z-q.z);
    if(h>maxHorizontal||v>maxVertical||!clearCell(i))continue;
    float candidate=h+v*2;if(candidate<score){score=candidate;best=i;}
   }
  }
  return best;
 }
 bool footprint(Vec p)const{
  if(!supportAt(p.x,p.y,p.z))return false;
  for(int sample=0;sample<8;++sample){float a=sample*.785398163f;if(!supportAt(p.x+std::cos(a)*radius,p.y+std::sin(a)*radius,p.z))return false;}
  return true;
 }
public:
 void abandon(){search.reset();}
 bool pending()const{return search.has_value();}
 void clear(){abandon();mesh.clear();sources.clear();groups.clear();cells.clear();columns.clear();clearance.clear();edges.clear();}
 void update(std::vector<nav::Triangle> input,Vec focus,float range=2600.f){
  auto previousGroups=groups.size();abandon();mesh=std::move(input);sources.clear();stats={};
  int gx0=groupCoordinate(coordinate(focus.x-range)),gx1=groupCoordinate(coordinate(focus.x+range));
  int gy0=groupCoordinate(coordinate(focus.y-range)),gy1=groupCoordinate(coordinate(focus.y+range));
  for(std::size_t i=0;i<mesh.size();++i){auto& t=mesh[i];if(!nav::walkable(t))continue;
   float minX=t.vertices[0].x,maxX=minX,minY=t.vertices[0].y,maxY=minY;
   for(auto p:t.vertices){minX=(std::min)(minX,p.x);maxX=(std::max)(maxX,p.x);minY=(std::min)(minY,p.y);maxY=(std::max)(maxY,p.y);}
   for(int x=(std::max)(gx0,groupCoordinate(coordinate(minX)));x<=(std::min)(gx1,groupCoordinate(coordinate(maxX)));++x)
    for(int y=(std::max)(gy0,groupCoordinate(coordinate(minY)));y<=(std::min)(gy1,groupCoordinate(coordinate(maxY)));++y)sources[key(x,y)].push_back(i);
  }
  for(auto it=groups.begin();it!=groups.end();)if(!sources.contains(it->first))it=groups.erase(it);else ++it;
  for(const auto& [id,indices]:sources){
   std::uint64_t signature=1469598103934665603ull;
   auto mix=[&](std::uint64_t v){signature=(signature^v)*1099511628211ull;};
   for(auto index:indices){mix(mesh[index].key);for(auto p:mesh[index].vertices){mix(std::bit_cast<std::uint32_t>(p.x));mix(std::bit_cast<std::uint32_t>(p.y));mix(std::bit_cast<std::uint32_t>(p.z));}}
   auto existing=groups.find(id);if(existing!=groups.end()&&existing->second.signature==signature){++stats.reused;continue;}
   int gx=static_cast<std::int32_t>(id>>32),gy=static_cast<std::int32_t>(id);
   Group group;group.signature=signature;
   for(int dx=0;dx<groupSize;++dx)for(int dy=0;dy<groupSize;++dy){
    int x=gx*groupSize+dx,y=gy*groupSize+dy;float wx=(x+.5f)*spacing,wy=(y+.5f)*spacing;
    std::vector<float> heights;
    for(auto index:indices){const auto& t=mesh[index];auto p=nav::verticalPoint({wx,wy,0},t);if((nav::closest(p,t)-p).length()>1.f)continue;
     bool duplicate=false;for(float z:heights)if(std::abs(z-p.z)<8.f)duplicate=true;
     if(!duplicate)heights.push_back(p.z);
    }
    for(float z:heights)group.cells.push_back({x,y,{wx,wy,z}});
   }
   groups[id]=std::move(group);++stats.built;
  }
  if(!cells.empty()&&stats.built==0&&groups.size()==previousGroups){stats.groups=groups.size();stats.cells=cells.size();edges.clear();return;}
  cells.clear();columns.clear();
  for(const auto& [id,group]:groups)for(const auto& cell:group.cells){columns[key(cell.x,cell.y)].push_back(cells.size());cells.push_back(cell);}
  clearance.assign(cells.size(),-1);edges.clear();stats.groups=groups.size();stats.cells=cells.size();
 }
 const std::vector<Cell>& allCells()const{return cells;}
 const Stats& statistics()const{return stats;}
 std::vector<Cell> supportedCells(){std::vector<Cell> result;for(std::size_t i=0;i<cells.size();++i)if(clearCell(i))result.push_back(cells[i]);return result;}
 using Validator=std::function<bool(Vec,Vec,nav::Traversal)>;
 bool canWalk(Vec a,Vec b,const Validator& validate)const{
  int steps=static_cast<int>(std::ceil((b-a).length()/12.f));
  for(int i=0;i<=steps;++i)if(!footprint(a+(b-a)*(static_cast<float>(i)/(std::max)(1,steps))))return false;
  return validate(a,b,nav::Traversal::walk);
 }
 Result plan(Vec start,Vec finish,const Validator& validate,float maxDrop,float goalRadius=0,
             std::size_t sliceExpansions=24000,std::chrono::milliseconds sliceTime=(std::chrono::milliseconds::max)(),
             const std::function<bool(Vec)>& goalVisible={}){
  Result result;result.stats=stats;
  auto started=std::chrono::steady_clock::now();
  if(search&&((search->start-start).length()>8.f||(search->finish-finish).length()>32.f||search->goalRadius!=goalRadius))abandon();
  if(!search){
  // Actor/object orders end in an approach region, never inside the target's collider.
  auto from=nearest(start,64.f,80.f),to=nearest(finish,48.f,80.f);
  if(!from){result.reason="start-has-no-radius-clear-cell";return result;}
  if(!to&&goalRadius==0){result.reason="destination-has-no-radius-clear-cell";return result;}
  if(std::abs(start.z-cells[*from].position.z)>32.f||!validate(start,cells[*from].position,nav::Traversal::walk)){result.reason="start-to-grid-blocked";return result;}
  if(goalRadius==0&&(!footprint(finish)||std::abs(finish.z-cells[*to].position.z)>32.f||!validate(cells[*to].position,finish,nav::Traversal::walk))){result.reason="grid-to-destination-blocked";return result;}
  if(goalRadius==0){
   bool supported=true;int steps=static_cast<int>(std::ceil((finish-start).length()/12.f));
   for(int i=0;i<=steps;++i){auto at=start+(finish-start)*(static_cast<float>(i)/(std::max)(1,steps));if(!footprint(at)){supported=false;break;}}
   if(supported&&validate(start,finish,nav::Traversal::walk)){
    result.route.points={start,finish};result.route.traversal={nav::Traversal::walk,nav::Traversal::walk};result.reason="direct-walk";return result;
   }
  }
  search.emplace();auto& s=*search;s.start=start;s.finish=finish;s.goalRadius=goalRadius;s.from=*from;s.to=to.value_or(cells.size());
  s.cost.assign(cells.size(),std::numeric_limits<float>::infinity());s.parent.assign(cells.size(),cells.size());s.actions.resize(cells.size());
  s.cost[*from]=0;s.queue.push({0,0,*from});
  // Physics results are useful across slices of this search, but must not outlive moving actors.
  edges.clear();
  }
  auto& s=*search;auto& queue=s.queue;auto& cost=s.cost;auto& parent=s.parent;auto& actions=s.actions;
  auto from=s.from;auto to=s.to;finish=s.finish;start=s.start;std::size_t sliceCount=0;
  auto canEdge=[&](std::size_t a,std::size_t b,nav::Traversal type){
   auto id=(static_cast<std::uint64_t>(a)<<32)|b;auto found=edges.find(id);if(found!=edges.end())return found->second;
   result.stats.clearanceChecks=++s.checks;return edges[id]=validate(cells[a].position,cells[b].position,type);
  };
  while(!queue.empty()&&s.expanded<24000){
   if(sliceCount>=sliceExpansions||(sliceCount>0&&sliceTime!=(std::chrono::milliseconds::max)()&&std::chrono::steady_clock::now()-started>=sliceTime)){
    result.pending=true;result.reason="search-pending";result.stats.expanded=s.expanded;return result;
   }
   auto current=queue.top();queue.pop();if(current.g>cost[current.i])continue;++s.expanded;++sliceCount;result.stats.expanded=s.expanded;
   auto goal=cells[current.i].position;
   if(goalRadius>0?(planarDistance(goal,finish)<=goalRadius&&std::abs(goal.z-finish.z)<=64.f&&(!goalVisible||goalVisible(goal))):current.i==to){
    std::vector<std::size_t> chain;for(auto i=current.i;;i=parent[i]){chain.push_back(i);if(i==from)break;}
    result.route.points.push_back(start);result.route.traversal.push_back(nav::Traversal::walk);
    for(auto it=chain.rbegin();it!=chain.rend();++it){result.route.points.push_back(cells[*it].position);result.route.traversal.push_back(actions[*it]);}
    // Greedy any-angle simplification uses circular support and physical checks,
    // and never smooths across a jump/drop action.
    nav::Route smooth;smooth.points.push_back(result.route.points[0]);smooth.traversal.push_back(nav::Traversal::walk);
    for(std::size_t i=0;i+1<result.route.points.size();){
     std::size_t end=(std::min)(i+16,result.route.points.size()-1),next=i+1;
     for(auto j=i+1;j<=end;++j)if(result.route.traversal[j]!=nav::Traversal::walk){end=j-1;break;}
     for(auto j=end;j>i+1;--j){
      Vec a=result.route.points[i],b=result.route.points[j];int steps=static_cast<int>(std::ceil((b-a).length()/12.f));bool supported=true;
      for(int s=1;s<steps;++s){auto p=a+(b-a)*(static_cast<float>(s)/steps);if(!footprint(p)){supported=false;break;}}
      if(supported&&validate(a,b,nav::Traversal::walk)){next=j;break;}
     }
     smooth.points.push_back(result.route.points[next]);smooth.traversal.push_back(result.route.traversal[next]);i=next;
    }
    if(goalRadius==0&&planarDistance(smooth.points.back(),finish)<48.f&&std::abs(smooth.points.back().z-finish.z)<32.f&&validate(smooth.points.back(),finish,nav::Traversal::walk)){smooth.points.push_back(finish);smooth.traversal.push_back(nav::Traversal::walk);}
    smooth.expanded=result.stats.expanded;result.route=std::move(smooth);result.reason="accepted";abandon();return result;
   }
   const auto& cell=cells[current.i];
   for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy){if(!dx&&!dy)continue;
    bool adjacentWalk=false;
    for(int stride=1;stride<=4;++stride){
     if(stride>1&&adjacentWalk)break;
     auto found=columns.find(key(cell.x+dx*stride,cell.y+dy*stride));if(found==columns.end())continue;
     for(auto next:found->second){auto a=cell.position,b=cells[next].position;float dz=b.z-a.z;
      nav::Traversal action=nav::Traversal::walk;
      if(stride>1||std::abs(dz)>32.f){action=dz < -20?nav::Traversal::drop:nav::Traversal::jump;if(!nav::traversable(a,b,action,maxDrop))continue;}
      if(!clearCell(next))continue;
      if(action==nav::Traversal::walk&&dx&&dy){if(!nearest({a.x+dx*spacing,a.y,a.z},4.f,40.f)||!nearest({a.x,a.y+dy*spacing,a.z},4.f,40.f))continue;}
      if(!canEdge(current.i,next,action))continue;
      if(action==nav::Traversal::walk)adjacentWalk=true;
      float proposed=current.g+(b-a).length()+(action==nav::Traversal::walk?0:250.f+std::abs(dz)*.5f);
      if(proposed>=cost[next])continue;cost[next]=proposed;parent[next]=current.i;actions[next]=action;
      queue.push({proposed+(std::max)(0.f,(b-finish).length()-goalRadius),proposed,next});
     }
    }
   }
  }
  result.reason=result.stats.expanded>=24000?"search-budget-exhausted":"no-connected-grid-route";abandon();return result;
 }
};
}
