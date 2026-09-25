#pragma once
#include <algorithm>
#include <string>
#include <vector>
namespace scape::menu {
enum class Action{walk,attack,talk,activate,stop,close};
struct Row{Action action;std::string label;};
inline std::vector<Row> actorRows(bool dead,bool hostile,bool canTalk=true){
 if(dead)return {{Action::activate,"Search"}};
 std::vector<Row> result;if(!hostile&&canTalk)result.push_back({Action::talk,"Talk"});
 else if(!hostile)result.push_back({Action::activate,"Interact"});
 result.push_back({Action::attack,"Attack"});return result;
}
struct Layout{
 float x{},y{},width{.28f},rowHeight{.045f},header{.055f};std::size_t rows{};
 float height()const{return header+rowHeight*rows+.012f;}
 void place(float cursorX,float cursorY){x=std::clamp(cursorX,.01f,1.f-width-.01f);y=std::clamp(cursorY,.01f,(std::max)(.01f,1.f-height()-.01f));}
 int hit(float px,float py)const{
  if(px<x||px>=x+width||py<y+header||py>=y+header+rowHeight*rows)return -1;
  return static_cast<int>((py-y-header)/rowHeight);
 }
};
}
