#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
namespace scape::menu {
enum class Action{walk,attack,talk,activate,gather,stop,close};
struct Row{Action action;std::string label;std::uint32_t target{};};
inline std::vector<Row> actorRows(bool dead,bool hostile,bool canTalk=true){
 if(dead)return {{Action::activate,"Search"}};
 std::vector<Row> result;if(!hostile&&canTalk)result.push_back({Action::talk,"Talk"});
 else if(!hostile)result.push_back({Action::activate,"Interact"});
 result.push_back({Action::attack,"Attack"});return result;
}
struct Layout{
 float x{},y{},width{.28f},rowHeight{.045f},header{.055f};std::size_t rows{},offset{};
 static constexpr std::size_t pageSize=12;
 std::size_t visibleRows()const{return (std::min)(rows,pageSize);}
 void scroll(int step){if(step>0&&offset) --offset;else if(step<0&&offset+visibleRows()<rows) ++offset;}
 float height()const{return header+rowHeight*visibleRows()+.012f;}
 void place(float cursorX,float cursorY){x=std::clamp(cursorX,.01f,1.f-width-.01f);y=std::clamp(cursorY,.01f,(std::max)(.01f,1.f-height()-.01f));}
 int hit(float px,float py)const{
  if(px<x||px>=x+width||py<y+header||py>=y+header+rowHeight*visibleRows())return -1;
  return static_cast<int>(offset)+static_cast<int>((py-y-header)/rowHeight);
 }
};
}
