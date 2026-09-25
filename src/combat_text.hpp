#pragma once
#include <unordered_map>
#include "system_text.hpp"
#include <mutex>
#include <chrono>

// Health deltas reflect health actually lost (including mitigation), rather
// than weapon base damage. Multiple hits in one update are displayed together.
namespace combat_text {
using Clock=std::chrono::steady_clock;
struct Popup {RE::NiPoint3 position;std::string text;bool player{},block{};Clock::time_point born;unsigned lane{};};
inline std::mutex mutex;
inline bool enabled{};
inline std::vector<Popup> popups;
inline std::unordered_map<RE::FormID,float> health;
inline unsigned sequence{};
inline void add(RE::Actor* actor,std::string text,bool block){
 auto position=actor->GetPosition();position.z+=110.f;
 if(popups.size()>=48)popups.erase(popups.begin());
 popups.push_back({position,std::move(text),actor==RE::PlayerCharacter::GetSingleton(),block,Clock::now(),sequence++%3});
 spdlog::info("COMBAT TEXT actor={:08X} text={} blocked={}",actor->GetFormID(),popups.back().text,block);
}
class Hits final:public RE::BSTEventSink<RE::TESHitEvent>{
public:
 RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* event,RE::BSTEventSource<RE::TESHitEvent>*)override{
  if(!event||!event->target||!event->flags.any(RE::TESHitEvent::Flag::kHitBlocked))return RE::BSEventNotifyControl::kContinue;
  auto actor=event->target->As<RE::Actor>();if(!actor)return RE::BSEventNotifyControl::kContinue;
  std::lock_guard lock(mutex);
  if(enabled&&health.contains(actor->GetFormID()))add(actor,"BLOCK",true);
  return RE::BSEventNotifyControl::kContinue;
 }
};
inline Hits hits;
inline void update(bool active){
 std::lock_guard lock(mutex);enabled=active;
 if(!active){health.clear();popups.clear();return;}
 auto player=RE::PlayerCharacter::GetSingleton();if(!player)return;
 std::unordered_map<RE::FormID,float> current;
 auto sample=[&](RE::Actor* actor){
  if(!actor||!actor->Get3D()||actor->GetWorldspace()!=player->GetWorldspace()||
     (!player->GetWorldspace()&&actor->GetParentCell()!=player->GetParentCell())||
     actor->GetPosition().GetDistance(player->GetPosition())>3000.f)return;
  float value=actor->GetActorValue(RE::ActorValue::kHealth);if(!std::isfinite(value))return;
  value=(std::max)(0.f,value);auto id=actor->GetFormID();current[id]=value;
  auto previous=health.find(id);
  if(previous!=health.end()){
   float lost=previous->second-value;
   if(lost>=.05f)add(actor,lost<1.f?fmt::format("{:.1f}",lost):fmt::format("{:.0f}",lost),false);
  }
 };
 sample(player);
 if(auto processes=RE::ProcessLists::GetSingleton())processes->ForEachHighActor([&](RE::Actor* actor){if(actor!=player)sample(actor);return RE::BSContainer::ForEachResult::kContinue;});
 health=std::move(current);
 auto now=Clock::now();std::erase_if(popups,[&](const Popup& p){return now-p.born>std::chrono::milliseconds(1250);});
}
inline void draw(RE::GFxValue& root,RE::NiCamera* camera,const RE::GRectF& rect,bool visible){
 std::vector<Popup> snapshot;{std::lock_guard lock(mutex);snapshot=popups;}
 auto now=Clock::now();
 static std::array<std::string,48> rendered;
 for(std::size_t i=0;i<48;++i){
  auto name="SkyrimScapeDamage"+std::to_string(i);RE::GFxValue label;
  bool exists=root.GetMember(name.c_str(),&label)&&label.IsDisplayObject();
  if(!exists&&i>=snapshot.size())continue;
  if(!exists){
   root.CreateEmptyMovieClip(&label,name.c_str(),static_cast<std::int32_t>(16100+i));
  }
  if(!label.IsDisplayObject())continue;
  RE::GFxValue::DisplayInfo info;info.SetVisible(false);
  if(visible&&camera&&i<snapshot.size()){
   const auto& p=snapshot[i];float age=std::chrono::duration<float>(now-p.born).count(),x{},y{},z{};
   if(age>=0&&age<1.25f&&camera->WorldPtToScreenPt3(p.position,x,y,z,1e-5f)&&std::isfinite(x)&&std::isfinite(y)&&x>=0&&x<=1&&y>=0&&y<=1){
    auto key=p.text+(p.block?"B":p.player?"P":"E");
    if(!exists||rendered[i]!=key){system_text::draw(label,p.text,3.,p.block?0x71D9FF:p.player?0xFF6868:0xFFE8A0,true);rendered[i]=key;}
    info.SetPosition(rect.left+x*(rect.right-rect.left)-system_text::width(p.text,3.)*.5+(static_cast<int>(p.lane)-1)*20,rect.top+(1-y)*(rect.bottom-rect.top)-age*48-p.lane*14);
    info.SetAlpha(std::clamp((1.25f-age)/.4f,0.f,1.f)*100);info.SetVisible(true);
   }
  }
  label.SetDisplayInfo(info);
 }
}
}
