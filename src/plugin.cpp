#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <chrono>
#include <mutex>
#include "control_math.hpp"
#include "native_navigation.hpp"
#include "terrain_overlay.hpp"
#include "combat_text.hpp"
namespace {
using Clock=std::chrono::steady_clock;
scape::Vec vec(RE::NiPoint3 p){return {p.x,p.y,p.z};}
RE::NiPoint3 point(scape::Vec p){return {p.x,p.y,p.z};}
enum class Order{none,walk,interact,attack};
struct State{
 bool enabled{},middle{},ownsMovement{},attackHeld{};
 bool overlay{true};Clock::time_point nextOverlay{};
 scape::Orbit orbit;
 float cursorX{.5f},cursorY{.5f};
 Order order{Order::none};
 RE::ObjectRefHandle target;
 scape::Vec destination{},lastPosition{};
 Clock::time_point lastProgress{},nextAttack{};
 RE::FormID cell{};
 RE::FormID world{};
 std::vector<scape::Vec> route;
 std::vector<scape::nav::Traversal> traversal;
 std::size_t activeTraversal{(std::numeric_limits<std::size_t>::max)()};
 Clock::time_point traversalStarted{};
 bool traversalAirborne{};
 std::size_t waypoint{};
 scape::Vec plannedTarget{};
 Clock::time_point nextPlan{};
 bool planning{};
 Clock::time_point planStarted{};std::size_t planSlices{};double maxPlanSliceMs{};
 Clock::time_point nextMovementLog{};
 bool savedRunning{},runningOwned{};
 Clock::time_point nextTerrainCheck{},nextJump{};
}state;
struct Display{bool enabled{};float x{.5f},y{.5f};bool overlay{};}display;
struct TerrainDisplay{std::vector<scape::grid::Cell> cells;};
std::shared_ptr<const TerrainDisplay> terrainDisplay;
std::vector<scape::Vec> displayedRoute;
struct ClickFeedback{float x{},y{};bool action{};Clock::time_point when{};}clickFeedback;
std::mutex displayMutex;
void rejectedClick(){std::lock_guard lock(displayMutex);clickFeedback={state.cursorX,state.cursorY,true,Clock::now()};}
bool gameplay(){
 auto ui=RE::UI::GetSingleton();auto p=RE::PlayerCharacter::GetSingleton();
 auto controls=RE::ControlMap::GetSingleton();auto camera=RE::PlayerCamera::GetSingleton();
 if(!ui||!p||!controls||!camera||!p->Get3D()||p->IsDead()||ui->GameIsPaused())return false;
 if(!controls->IsMovementControlsEnabled()||p->GetSitSleepState()!=RE::SIT_SLEEP_STATE::kNormal)return false;
 for(auto name:{"Dialogue Menu","InventoryMenu","MagicMenu","ContainerMenu","BarterMenu","Crafting Menu","MapMenu","Console","TweenMenu","Main Menu","Loading Menu","RaceSex Menu","Book Menu","Lockpicking Menu"})if(ui->IsMenuOpen(name))return false;
 return camera->IsInThirdPerson()||camera->IsInFirstPerson();
}
void attackButton(RE::PlayerControls* controls,bool down){
 if(!controls||!controls->attackBlockHandler)return;
 auto e=RE::ButtonEvent::Create(RE::INPUT_DEVICE::kMouse,RE::UserEvents::GetSingleton()->rightAttack,0,down?1.f:0.f,down?0.f:.1f);
 if(e){controls->attackBlockHandler->ProcessButton(e,&controls->data);delete e;}
 state.attackHeld=down;
}
void cancel(RE::PlayerControls* c){
 native_navigation::gridCache().grid.abandon();state.planning=false;
 if(state.attackHeld)attackButton(c,false);
 if(c&&state.ownsMovement){c->data.moveInputVec={0,0};c->data.prevMoveVec={0,0};state.ownsMovement=false;}
 if(c&&state.runningOwned){c->data.running=state.savedRunning;state.runningOwned=false;}
 state.order=Order::none;state.target={};
 state.route.clear();state.waypoint=0;
 state.traversal.clear();state.activeTraversal=(std::numeric_limits<std::size_t>::max)();
}
struct Hit{bool valid{};scape::Vec position;RE::TESObjectREFR* reference{};float normalZ{};};
Hit cast(RE::NiPoint3 start,RE::NiPoint3 end,bool logIgnored=false){
 auto p=RE::PlayerCharacter::GetSingleton();auto cell=p?p->GetParentCell():nullptr;auto world=cell?cell->GetbhkWorld():nullptr;
 if(!world)return {};
 RE::bhkPickData pick{};const float scale=RE::bhkWorld::GetWorldScale();
 pick.rayInput.from=RE::hkVector4(start*scale);pick.rayInput.to=RE::hkVector4(end*scale);
 pick.rayInput.filterInfo.filter=static_cast<std::uint32_t>(RE::COL_LAYER::kLOS);
 if(auto controller=p->GetCharController())pick.rayInput.filterInfo.SetSystemGroup(controller->collisionFilterGroup);
 RE::BSReadLockGuard lock(world->worldLock);
 world->PickObject(pick);if(!pick.rayOutput.HasHit())return {};
 auto ignored=[p](RE::TESObjectREFR* ref){
  // Skyrim.esm 0002F245 is FXcameraAttachEffectsACT. Its invisible volume
  // was intercepting dozens of unrelated clicks and also clearance rays.
  return ref&&(ref==p||(ref->GetBaseObject()&&ref->GetBaseObject()->GetFormID()==0x0002F245));
 };
 auto firstRef=RE::TESHavokUtilities::FindCollidableRef(*pick.rayOutput.rootCollidable);
 if(ignored(firstRef)){
  RE::hkpAllRayHitCollector collector;RE::bhkPickData all{};all.rayInput=pick.rayInput;all.allRayHitCollector=&collector;world->PickObject(all);
  const RE::hkpWorldRayCastOutput* nearest=nullptr;
  for(const auto& hit:collector.hits){
   if(!hit.HasHit()||!hit.rootCollidable)continue;
   if(ignored(RE::TESHavokUtilities::FindCollidableRef(*hit.rootCollidable)))continue;
   if(!nearest||hit.hitFraction<nearest->hitFraction)nearest=&hit;
  }
  if(logIgnored)spdlog::info("PICK skipped invisible camera/self reference {:08X}; remaining hit={}",firstRef->GetFormID(),nearest!=nullptr);
  if(!nearest)return {};pick.rayOutput=*nearest;
 }
 alignas(16) float normal[4];_mm_store_ps(normal,pick.rayOutput.normal.quad);
 return {true,vec(start+(end-start)*pick.rayOutput.hitFraction),RE::TESHavokUtilities::FindCollidableRef(*pick.rayOutput.rootCollidable),normal[2]};
}
bool clearTraversal(scape::Vec takeoff,scape::Vec landing,scape::nav::Traversal type){
 if(type==scape::nav::Traversal::walk){
  auto delta=landing-takeoff;float horizontal=std::hypot(delta.x,delta.y);
  if(horizontal<1.f)return std::abs(delta.z)<35.f;
  auto side=scape::Vec{-delta.y/horizontal,delta.x/horizontal,0}*scape::grid::radius;
  for(float offset:{-1.f,0.f,1.f})for(float height:{45.f,110.f}){
   auto a=takeoff+side*offset+scape::Vec{0,0,height},b=landing+side*offset+scape::Vec{0,0,height};
   if(cast(point(a),point(b)).valid)return false;
  }
  return true;
 }
 float maxDrop=180.f;
 if(auto settings=RE::GameSettingCollection::GetSingleton())if(auto setting=settings->GetSetting("fJumpFallHeightMin")){
  auto threshold=setting->GetFloat();if(std::isfinite(threshold)&&threshold>=40.f)maxDrop=(std::min)(512.f,threshold*.75f);
 }
 if(!scape::nav::traversable(takeoff,landing,type,maxDrop))return false;
 for(auto offset:{scape::Vec{0,0,0},scape::Vec{16,0,0},scape::Vec{-16,0,0},scape::Vec{0,16,0},scape::Vec{0,-16,0}}){
  auto at=landing+offset;auto floor=cast(point(at+scape::Vec{0,0,28}),point(at-scape::Vec{0,0,36}));
  if(!floor.valid||floor.normalZ<.65f||std::abs(floor.position.z-at.z)>28.f)return false;
  if(cast(point(floor.position+scape::Vec{0,0,8}),point(floor.position+scape::Vec{0,0,140})).valid)return false;
 }
 auto delta=landing-takeoff;auto side=scape::Vec{-delta.y,delta.x,0};side=side*(16.f/side.length());
 for(float lateral:{-1.f,0.f,1.f})for(float height:{12.f,70.f,130.f}){
  auto previous=takeoff+side*lateral+scape::Vec{0,0,height};
  for(int step=1;step<=8;++step){
   float t=step/8.f;auto next=takeoff+delta*t+side*lateral+scape::Vec{0,0,height};
   if(type==scape::nav::Traversal::jump)next.z+=70.f*4.f*t*(1.f-t);
   else{
    // Walk beyond the lip before falling; a straight descending ray would hit
    // the upper floor and incorrectly reject every ordinary walk-off ledge.
    float fall=std::clamp((t-.45f)/.55f,0.f,1.f);
    next.z=takeoff.z+height+(landing.z-takeoff.z)*fall*fall;
   }
   if(cast(point(previous),point(next)).valid)return false;previous=next;
  }
 }
 return true;
}
RE::NiCamera* findCamera(RE::NiAVObject* o){
 if(!o)return nullptr;if(auto c=netimmerse_cast<RE::NiCamera*>(o))return c;
 if(auto n=o->AsNode())for(auto& child:n->GetChildren())if(auto c=findCamera(child.get()))return c;
 return nullptr;
}
bool planRoute(scape::Vec position){
 auto started=Clock::now();
 if(!state.planning){state.planStarted=started;state.planSlices=0;state.maxPlanSliceMs=0;}
 auto goalVisible=[](scape::Vec approach){
  auto target=state.target.get();if(!target)return false;
  auto hit=cast(point(approach+scape::Vec{0,0,60}),point(state.destination+scape::Vec{0,0,60}));
  return !hit.valid||hit.reference==target.get();
 };
 auto result=native_navigation::plan(position,state.destination,state.order==Order::walk,clearTraversal,goalVisible);
 ++state.planSlices;auto elapsed=std::chrono::duration<double,std::milli>(Clock::now()-started).count();state.maxPlanSliceMs=(std::max)(state.maxPlanSliceMs,elapsed);
 if(elapsed>12)spdlog::info("PLAN slow slice {:.2f} ms expanded={} pending={}",elapsed,result.stats.expanded,result.pending);
 state.planning=result.pending;state.nextPlan=Clock::now()+std::chrono::milliseconds(result.pending?0:750);
 if(result.pending)return false;
 auto& route=result.route;
 if(route.points.empty()){spdlog::info("No usable grid route: slices={} maxSliceMs={:.2f}; see preceding GRID reason",state.planSlices,state.maxPlanSliceMs);return false;}
 state.route=std::move(route.points);state.waypoint=0;state.plannedTarget=state.destination;
 state.traversal=std::move(route.traversal);state.activeTraversal=(std::numeric_limits<std::size_t>::max)();
 state.nextPlan=Clock::now()+std::chrono::milliseconds(750);
 spdlog::info("Planned {} waypoints, expanded {} grid cells; slices={} maxSliceMs={:.2f} elapsedMs={:.2f}",state.route.size(),route.expanded,state.planSlices,state.maxPlanSliceMs,std::chrono::duration<double,std::milli>(Clock::now()-state.planStarted).count());
 for(std::size_t i=0;i<state.route.size();++i){auto p=state.route[i];spdlog::info("ROUTE point {} type={} position {:.2f} {:.2f} {:.2f}",i,static_cast<int>(state.traversal[i]),p.x,p.y,p.z);}
 return true;
}
float orderRange(){return state.order==Order::walk?24.f:state.order==Order::attack?110.f:130.f;}
bool actionInReach(RE::PlayerCharacter* player,RE::TESObjectREFR* target){
 if(!target||!scape::reached(vec(player->GetPosition()),vec(target->GetPosition()),orderRange()))return false;
 bool unused=false;return player->HasLineOfSight(target,unused);
}
void click(){
 static std::uint64_t clickSequence=0;auto sequence=++clickSequence;
 spdlog::info("CLICK {} received screen {:.4f} {:.4f}",sequence,state.cursorX,state.cursorY);
 auto pc=RE::PlayerCamera::GetSingleton();auto camera=pc?findCamera(pc->cameraRoot.get()):nullptr;if(!camera)return;
 RE::NiPoint3 start,direction;
 if(!camera->WindowPointToRay(static_cast<int>(state.cursorX*10000),static_cast<int>(state.cursorY*10000),start,direction,10000,10000)){rejectedClick();return;}
 auto hit=cast(start,start+direction*12000.f,true);auto p=RE::PlayerCharacter::GetSingleton();
 spdlog::info("CLICK {} ray valid={} ref={:08X} collision {:.2f} {:.2f} {:.2f} normalZ={:.3f}",sequence,hit.valid,hit.reference?hit.reference->GetFormID():0,hit.position.x,hit.position.y,hit.position.z,hit.normalZ);
 if(!hit.valid||hit.reference==p){rejectedClick();return;}
 cancel(RE::PlayerControls::GetSingleton());state.destination=hit.position;state.order=Order::walk;
 if(hit.reference&&hit.reference->GetBaseObject()){
  auto actor=hit.reference->As<RE::Actor>();auto type=hit.reference->GetBaseObject()->GetFormType();
  if(actor&&!actor->IsDead()&&actor->IsHostileToActor(p))state.order=Order::attack;
  else if(actor||type==RE::FormType::Door||type==RE::FormType::Container||type==RE::FormType::Activator||type==RE::FormType::Furniture||hit.reference->GetBaseObject()->IsInventoryObject())state.order=Order::interact;
  if(state.order!=Order::walk){state.target=hit.reference->GetHandle();state.destination=vec(hit.reference->GetPosition());}
 }
 state.lastPosition=vec(p->GetPosition());state.lastProgress=Clock::now();state.nextAttack={};
 if(state.order==Order::walk||!actionInReach(p,hit.reference)){
  if(!planRoute(state.lastPosition)&&!state.planning&&state.order==Order::walk){rejectedClick();cancel(RE::PlayerControls::GetSingleton());return;}
 }else{state.plannedTarget=state.destination;state.nextPlan={};}
 if(state.order==Order::walk&&!state.route.empty())state.destination=state.route.back();
 spdlog::info("Click screen {:.3f} {:.3f}, collision {:.1f} {:.1f} {:.1f}, selected {:.1f} {:.1f} {:.1f}",state.cursorX,state.cursorY,hit.position.x,hit.position.y,hit.position.z,state.destination.x,state.destination.y,state.destination.z);
 {std::lock_guard lock(displayMutex);clickFeedback={state.cursorX,state.cursorY,state.order!=Order::walk,Clock::now()};}
 spdlog::info("Order {} target {:08X} destination {:.1f} {:.1f} {:.1f}",static_cast<int>(state.order),hit.reference?hit.reference->GetFormID():0,state.destination.x,state.destination.y,state.destination.z);
}
bool followTraversal(RE::PlayerControls* controls,scape::Vec position){
 auto index=state.waypoint;auto type=state.traversal[index];
 auto player=RE::PlayerCharacter::GetSingleton();auto controller=player->GetCharController();
 bool grounded=controller&&controller->flags.any(RE::CHARACTER_FLAGS::kSupport)&&!player->IsInJumpState();
 auto now=Clock::now();
 if(state.activeTraversal!=index){
  if(index==0||!grounded||scape::planarDistance(position,state.route[index-1])>35.f||
     std::abs(position.z-state.route[index-1].z)>35.f||!clearTraversal(state.route[index-1],state.route[index],type)){
   spdlog::info("Traversal cancelled: takeoff or landing no longer valid");cancel(controls);return false;
  }
  if(type==scape::nav::Traversal::jump){
   if(!controls->jumpHandler||!RE::ControlMap::GetSingleton()->IsJumpingControlsEnabled()){cancel(controls);return false;}
   player->SetHeading(scape::heading(position,state.route[index]));
   auto event=RE::ButtonEvent::Create(RE::INPUT_DEVICE::kKeyboard,RE::UserEvents::GetSingleton()->jump,0x39,1.f,0.f);
   if(!event){cancel(controls);return false;}
   controls->jumpHandler->ProcessButton(event,&controls->data);delete event;
  }
  state.activeTraversal=index;state.traversalStarted=now;state.traversalAirborne=false;
  spdlog::info("Planned {} started at waypoint {}",type==scape::nav::Traversal::jump?"jump":"drop",index);
 }
 if(!grounded)state.traversalAirborne=true;
 if(grounded&&state.traversalAirborne&&scape::planarDistance(position,state.route[index])<=32.f&&std::abs(position.z-state.route[index].z)<=28.f){
  spdlog::info("Planned traversal landed");++state.waypoint;state.activeTraversal=(std::numeric_limits<std::size_t>::max)();return true;
 }
 if(now-state.traversalStarted>std::chrono::seconds(3)){spdlog::info("Traversal cancelled: landing timeout");cancel(controls);return false;}
 return true;
}
void tick(RE::PlayerControls* c){
 auto p=RE::PlayerCharacter::GetSingleton();auto cell=p->GetParentCell();
 const auto world=p->GetWorldspace();const auto worldID=world?world->GetFormID():0;
 // A streamed exterior-cell boundary is part of one journey. Interiors/world changes cancel it.
 if(!cell||worldID!=state.world||(!worldID&&cell->GetFormID()!=state.cell))cancel(c);
 state.cell=cell?cell->GetFormID():0;state.world=worldID;
 if(state.attackHeld)attackButton(c,false);if(state.order==Order::none)return;
 auto now=Clock::now();auto position=vec(p->GetPosition());RE::NiPointer<RE::TESObjectREFR> target;
 if(state.order!=Order::walk){
  target=state.target.get();
  if(!target||!target->Get3D()||target->GetWorldspace()!=world||(!world&&target->GetParentCell()!=cell)){cancel(c);return;}
  state.destination=vec(target->GetPosition());
  if(state.order==Order::attack){auto actor=target->As<RE::Actor>();if(!actor||actor->IsDead()){spdlog::info("Attack order ended: target is dead or no longer an actor");cancel(c);return;}}
 }
 bool pendingTraversal=false;for(auto i=state.waypoint;i<state.traversal.size();++i)if(state.traversal[i]!=scape::nav::Traversal::walk)pendingTraversal=true;
 const bool arrived=!pendingTraversal&&state.activeTraversal==(std::numeric_limits<std::size_t>::max)()&&
  (state.order==Order::walk?(scape::reached(position,state.destination,orderRange())&&std::abs(position.z-state.destination.z)<=35.f):actionInReach(p,target.get()));
 if(!arrived){
  if((state.planning||state.order!=Order::walk)&&state.activeTraversal==(std::numeric_limits<std::size_t>::max)()&&now>=state.nextPlan&&
     (state.route.empty()||state.waypoint>=state.route.size()||(state.destination-state.plannedTarget).length()>80.f)){
   if(!planRoute(position)){
    c->data.moveInputVec={0,0};c->data.prevMoveVec={0,0};
    state.route.clear();state.traversal.clear();state.waypoint=0;state.lastProgress=now;
    if(!state.planning&&state.order==Order::walk){rejectedClick();cancel(c);}return;
   }
   if(state.order==Order::walk)state.destination=state.route.back();
  }
  while(state.waypoint<state.route.size()&&state.traversal[state.waypoint]==scape::nav::Traversal::walk){
   bool takeoff=state.waypoint+1<state.route.size()&&state.traversal[state.waypoint+1]!=scape::nav::Traversal::walk;
   bool close=scape::reached(position,state.route[state.waypoint],18.f)&&(!takeoff||std::abs(position.z-state.route[state.waypoint].z)<=28.f);
   bool passed=!takeoff&&state.waypoint>0&&state.waypoint+1<state.route.size()&&scape::passedWaypoint(position,state.route[state.waypoint-1],state.route[state.waypoint]);
   if(!close&&!passed)break;++state.waypoint;
  }
  if(state.waypoint<state.route.size()&&state.traversal[state.waypoint]!=scape::nav::Traversal::walk&&!followTraversal(c,position))return;
  if(state.waypoint>=state.route.size()){
   // Reaching a projected approach point is not proof that a distant object is in activation range.
   c->data.moveInputVec={0,0};c->data.prevMoveVec={0,0};
   if(state.order==Order::walk)cancel(c);
   return;
  }
  // Face the route and run forward, as requested. Keep the native movement
  // heading in sync this tick; the visible RuneScape orbit stays independent.
  auto desiredHeading=scape::heading(position,state.route[state.waypoint]);
  p->SetHeading(desiredHeading);
  RE::PlayerCamera::GetSingleton()->GetRuntimeData2().yaw=desiredHeading;
  auto cameraHeading=RE::PlayerCamera::GetSingleton()->GetRuntimeData2().yaw;
  auto movement=scape::cameraRelativeInput(desiredHeading,cameraHeading);
  if(state.activeTraversal!=(std::numeric_limits<std::size_t>::max)()){
   float speed=std::clamp(scape::planarDistance(position,state.route[state.waypoint])/70.f,0.f,1.f);
   movement.x*=speed;movement.y*=speed;
  }
  // Brake at the final point so native acceleration does not carry us past it.
  if(state.waypoint+1==state.route.size()){
   const float speed=std::clamp(scape::planarDistance(position,state.destination)/100.f,.3f,1.f);
   movement.x*=speed;movement.y*=speed;
  }
  c->data.moveInputVec={movement.x,movement.y};c->data.prevMoveVec=c->data.moveInputVec;state.ownsMovement=true;
  if(!state.runningOwned){state.savedRunning=c->data.running;state.runningOwned=true;}c->data.running=true;
  // Jump/drop input is issued only at explicit, revalidated route transitions.
  if(now>=state.nextMovementLog){
   spdlog::info("Movement pos {:.1f} {:.1f} {:.1f} heading {:.2f} native camera {:.2f} desired {:.2f} input {:.2f} {:.2f} waypoint {}/{}",position.x,position.y,position.z,p->GetAngleZ(),cameraHeading,desiredHeading,movement.x,movement.y,state.waypoint,state.route.size());
   state.nextMovementLog=now+std::chrono::milliseconds(500);
  }
  if(scape::planarDistance(position,state.lastPosition)>12){state.lastPosition=position;state.lastProgress=now;}
  if(now-state.lastProgress>std::chrono::seconds(3)){spdlog::info("Stopped: no movement progress");cancel(c);}return;
 }
 c->data.moveInputVec={0,0};c->data.prevMoveVec={0,0};state.ownsMovement=false;
 state.lastPosition=position;state.lastProgress=now;
 if(state.order==Order::attack){native_navigation::gridCache().grid.abandon();state.planning=false;state.route.clear();state.traversal.clear();state.waypoint=0;state.nextPlan={};}
 if(state.runningOwned){c->data.running=state.savedRunning;state.runningOwned=false;}
 if(state.order!=Order::walk)p->SetHeading(scape::heading(position,state.destination));
 if(state.order==Order::walk){spdlog::info("Walk destination reached");cancel(c);return;}
 if(state.order==Order::interact){auto ref=target;cancel(c);const bool result=ref->ActivateRef(p,0,nullptr,1,false);spdlog::info("Activation target {:08X} returned {}",ref->GetFormID(),result);return;}
 if(!RE::ControlMap::GetSingleton()->IsFightingControlsEnabled()){cancel(c);return;}
 if(!p->IsWeaponDrawn()){p->DrawWeaponMagicHands(true);return;}
 if(now>=state.nextAttack){
  auto actor=target->As<RE::Actor>();
  spdlog::info("Melee input target {:08X}, range {:.1f}, target health {:.1f}",target->GetFormID(),scape::planarDistance(position,state.destination),actor?actor->GetActorValue(RE::ActorValue::kHealth):0.f);
  attackButton(c,true);state.nextAttack=now+std::chrono::milliseconds(1000);
 }
}
using InputFn=RE::BSEventNotifyControl(*)(RE::PlayerControls*,RE::InputEvent* const*,RE::BSTEventSource<RE::InputEvent*>*);
REL::Relocation<InputFn> originalInput;
RE::BSEventNotifyControl input(RE::PlayerControls* c,RE::InputEvent* const* events,RE::BSTEventSource<RE::InputEvent*>* source){
 std::vector<std::pair<RE::InputEvent*,RE::InputEvent*>> links;
 RE::InputEvent* head=nullptr;RE::InputEvent** tail=&head;
 bool active=state.enabled&&gameplay();
 // Build a temporary filtered list; restore every changed engine-owned link afterward.
 for(auto e=events?*events:nullptr;e;){
  auto next=e->next;bool consume=false;
  if(auto b=e->AsButtonEvent()){
   auto id=b->GetIDCode();
   if(state.enabled&&e->GetDevice()==RE::INPUT_DEVICE::kKeyboard&&id==0x41&&b->IsDown()){
    state.overlay=!state.overlay;state.nextOverlay={};consume=true;
    spdlog::info("Terrain overlay {}",state.overlay?"on":"off");
   }
   if(e->GetDevice()==RE::INPUT_DEVICE::kKeyboard&&id==0x42&&b->IsDown()){
    state.enabled=!state.enabled;cancel(c);state.middle=false;state.nextOverlay={};
    if(state.enabled&&gameplay()){
     auto player=RE::PlayerCharacter::GetSingleton();state.orbit.yaw=player->GetAngleZ();
     state.cell=player->GetParentCell()?player->GetParentCell()->GetFormID():0;
     state.world=player->GetWorldspace()?player->GetWorldspace()->GetFormID():0;
     RE::PlayerCamera::GetSingleton()->ForceThirdPerson();
    }
    spdlog::info(state.enabled?"SkyrimScape on":"SkyrimScape off");consume=true;active=state.enabled&&gameplay();
   }
   if(active&&e->GetDevice()==RE::INPUT_DEVICE::kMouse){
    consume=true;if(id==2)state.middle=b->IsPressed();
    if(b->IsDown()){if(id==0)click();if(id==1)cancel(c);}
    if(auto steps=scape::wheelStep(id,b->Value());steps!=0){
     state.orbit.zoom(steps);
     spdlog::info("Mouse wheel {} -> camera distance {:.1f}",steps>0?"up/in":"down/out",state.orbit.distance);
    }
   }
   if(active&&e->GetDevice()==RE::INPUT_DEVICE::kKeyboard&&b->IsDown())if(id==0x11||id==0x1E||id==0x1F||id==0x20||id==0x01)cancel(c);
  }
  if(active&&e->GetEventType()==RE::INPUT_EVENT_TYPE::kMouseMove){
   auto m=e->AsMouseMoveEvent();consume=true;
   if(state.middle)state.orbit.rotate(static_cast<float>(m->mouseInputX),static_cast<float>(m->mouseInputY));
   else{state.cursorX=std::clamp(state.cursorX+m->mouseInputX/1600.f,0.f,1.f);state.cursorY=std::clamp(state.cursorY+m->mouseInputY/900.f,0.f,1.f);}
  }
  if(!consume){links.emplace_back(e,next);*tail=e;tail=&e->next;}e=next;
 }
 *tail=nullptr;
 if(!active){cancel(c);state.middle=false;}
 combat_text::update(active);
 auto result=originalInput(c,&head,source);
 for(auto [e,next]:links)e->next=next;
 if(active){
  c->data.lookInputVec={0,0};tick(c);
  if(state.overlay&&Clock::now()>=state.nextOverlay){
   auto started=Clock::now();
   auto terrain=std::make_shared<TerrainDisplay>();terrain->cells=native_navigation::refreshGrid().grid.supportedCells();
   spdlog::info("OVERLAY snapshot cells={} elapsedMs={:.2f}",terrain->cells.size(),std::chrono::duration<double,std::milli>(Clock::now()-started).count());
   {std::lock_guard lock(displayMutex);terrainDisplay=std::move(terrain);}
   state.nextOverlay=Clock::now()+std::chrono::seconds(2);
  }
 }
 {std::lock_guard lock(displayMutex);display={active,state.cursorX,state.cursorY,state.overlay};displayedRoute.clear();
  if(active&&state.order!=Order::none){displayedRoute.push_back(vec(RE::PlayerCharacter::GetSingleton()->GetPosition()));for(auto i=state.waypoint;i<state.route.size();++i)displayedRoute.push_back(state.route[i]);}
 }
 return result;
}
using RotationFn=void(*)(RE::ThirdPersonState*,RE::NiQuaternion&);
using TranslationFn=void(*)(RE::ThirdPersonState*,RE::NiPoint3&);
REL::Relocation<RotationFn> originalRotation;REL::Relocation<TranslationFn> originalTranslation;
void rotation(RE::ThirdPersonState* self,RE::NiQuaternion& output){
 if(!state.enabled||!gameplay()){originalRotation(self,output);return;}
 auto r=state.orbit.right(),f=state.orbit.forward(),u=state.orbit.up();RE::NiMatrix3 matrix;
 matrix.entry[0][0]=r.x;matrix.entry[1][0]=r.y;matrix.entry[2][0]=r.z;
 matrix.entry[0][1]=f.x;matrix.entry[1][1]=f.y;matrix.entry[2][1]=f.z;
 matrix.entry[0][2]=u.x;matrix.entry[1][2]=u.y;matrix.entry[2][2]=u.z;
 output=RE::NiQuaternion(matrix);
}
void translation(RE::ThirdPersonState* self,RE::NiPoint3& output){
 if(!state.enabled||!gameplay()){originalTranslation(self,output);return;}
 auto focus=vec(RE::PlayerCharacter::GetSingleton()->GetPosition())+scape::Vec{0,0,75};
 output=point(state.orbit.position(focus));
}
using CameraUpdateFn=void(*)(RE::ThirdPersonState*,RE::BSTSmartPointer<RE::TESCameraState>&);
REL::Relocation<CameraUpdateFn> originalCameraUpdate;
void cameraUpdate(RE::ThirdPersonState* self,RE::BSTSmartPointer<RE::TESCameraState>& next){
 originalCameraUpdate(self,next);
 if(!state.enabled||!gameplay())return;
 auto camera=RE::PlayerCamera::GetSingleton();
 if(camera->currentState.get()!=self||!camera->cameraRoot)return;
 // 1.7.104 ThirdPersonState::Update writes the camera node directly from its
 // fields without calling GetTranslation. A getter-only override cannot zoom.
 rotation(self,self->rotation);translation(self,self->translation);
 auto root=camera->cameraRoot.get();
 RE::NiMatrix3 matrix;self->rotation.ToRotation(matrix);
 if(root->parent){
  const auto inverse=root->parent->world.rotate.Transpose();
  root->local.rotate=inverse*matrix;
  root->local.translate=inverse*(self->translation-root->parent->world.translate)/root->parent->world.scale;
 }else{root->local.rotate=matrix;root->local.translate=self->translation;}
 RE::NiUpdateData update{};root->Update(update);
}
using HudFn=void(*)(RE::HUDMenu*,float,std::uint32_t);REL::Relocation<HudFn> originalHud;
void drawTerrain(RE::GFxValue& root,bool visible,const std::shared_ptr<const TerrainDisplay>& terrain,const std::vector<scape::Vec>& route,const RE::GRectF& rect){
 RE::GFxValue layer,label;
 if(!root.GetMember("SkyrimScapeTerrain",&layer)||!layer.IsDisplayObject())root.CreateEmptyMovieClip(&layer,"SkyrimScapeTerrain",15997);
 if(!root.GetMember("SkyrimScapeTerrainLegend",&label)){
  const std::array<RE::GFxValue,6> args{RE::GFxValue("SkyrimScapeTerrainLegend"),RE::GFxValue(15998.),RE::GFxValue(14.),RE::GFxValue(45.),RE::GFxValue(760.),RE::GFxValue(64.)};
  root.Invoke("createTextField",args);root.GetMember("SkyrimScapeTerrainLegend",&label);
  label.SetMember("embedFonts",RE::GFxValue(true));
  label.SetMember("htmlText",RE::GFxValue("<font face='$EverywhereFont' size='16' color='#FFFFFF'>F7: GRID | 32-unit cells | 20-unit circular clearance<br/><font color='#52ED88'>GREEN: supported cells, not guaranteed routes</font> | WHITE: selected route | X-ray view</font>"));
  label.SetMember("selectable",RE::GFxValue(false));
 }
 RE::GFxValue::DisplayInfo info;info.SetVisible(visible);
 if(label.IsDisplayObject())label.SetDisplayInfo(info);
 if(!layer.IsDisplayObject())return;layer.SetDisplayInfo(info);if(!visible||!terrain)return;
 static Clock::time_point nextDraw{},nextDiagnostic{};if(Clock::now()<nextDraw)return;nextDraw=Clock::now()+std::chrono::milliseconds(100);
 auto pc=RE::PlayerCamera::GetSingleton();auto camera=pc?findCamera(pc->cameraRoot.get()):nullptr;if(!camera)return;
 auto drawStarted=Clock::now();layer.Invoke("clear");
 // Separate shapes keep each Flash mesh small. Clear previous chunks even when
 // the next view contains fewer cells; the parent controls F7 visibility.
 static std::size_t previousChunks=0;RE::GFxValue gridLayer;
 for(std::size_t i=0;i<previousChunks;++i){RE::GFxValue old;auto name="grid"+std::to_string(i);if(layer.GetMember(name.c_str(),&old))old.Invoke("clear");}
 std::size_t drawn=0,chunks=0;
 const std::array<RE::GFxValue,3> gridStroke{RE::GFxValue(.65),RE::GFxValue(5434760.),RE::GFxValue(50.)};
 for(const auto& cell:terrain->cells){
  std::vector<scape::nav::ScreenPoint> polygon;bool valid=true;
  for(auto offset:{scape::Vec{-14,-14,2},scape::Vec{14,-14,2},scape::Vec{14,14,2},scape::Vec{-14,14,2}}){
   float x{},y{},z{};if(!camera->WorldPtToScreenPt3(point(cell.position+offset),x,y,z,1e-5f)||!std::isfinite(x)||!std::isfinite(y)){valid=false;break;}polygon.push_back({x,1-y});
  }
  if(!valid)continue;polygon=scape::nav::clipScreen(std::move(polygon));if(polygon.size()<3)continue;
  if(drawn%256==0){
   auto name="grid"+std::to_string(chunks);if(!layer.GetMember(name.c_str(),&gridLayer)||!gridLayer.IsDisplayObject())layer.CreateEmptyMovieClip(&gridLayer,name.c_str(),static_cast<std::int32_t>(chunks+1));
   if(!gridLayer.IsDisplayObject())break;
   gridLayer.Invoke("lineStyle",gridStroke);++chunks;
  }
  ++drawn;
  for(std::size_t i=0;i<=polygon.size();++i){auto p=polygon[i%polygon.size()];const std::array<RE::GFxValue,2> xy{RE::GFxValue(rect.left+p.x*(rect.right-rect.left)),RE::GFxValue(rect.top+p.y*(rect.bottom-rect.top))};gridLayer.Invoke(i?"lineTo":"moveTo",xy);}
 }
 previousChunks=chunks;
 if(Clock::now()>=nextDiagnostic){spdlog::info("OVERLAY supported={} drawn={} chunks={} drawMs={:.2f}",terrain->cells.size(),drawn,chunks,std::chrono::duration<double,std::milli>(Clock::now()-drawStarted).count());nextDiagnostic=Clock::now()+std::chrono::seconds(5);}
 const std::array<RE::GFxValue,3> routeStroke{RE::GFxValue(2.5),RE::GFxValue(16777215.),RE::GFxValue(100.)};layer.Invoke("lineStyle",routeStroke);
 bool previous=false;
 for(std::size_t i=0;i<route.size();++i){
  float x{},y{},z{};if(!camera->WorldPtToScreenPt3(point(route[i]+scape::Vec{0,0,5}),x,y,z,1e-5f)||x<0||x>1||y<0||y>1){previous=false;continue;}
  x=rect.left+x*(rect.right-rect.left);y=rect.top+(1-y)*(rect.bottom-rect.top);
  const std::array<RE::GFxValue,2> xy{RE::GFxValue(x),RE::GFxValue(y)};layer.Invoke(previous?"lineTo":"moveTo",xy);previous=true;
  if(i+1==route.size()){
   for(int corner=0;corner<=4;++corner){const float dx[4]={0,6,0,-6},dy[4]={-6,0,6,0};const std::array<RE::GFxValue,2> diamond{RE::GFxValue(x+dx[corner%4]),RE::GFxValue(y+dy[corner%4])};layer.Invoke(corner?"lineTo":"moveTo",diamond);}
  }
 }
}
void hud(RE::HUDMenu* self,float dt,std::uint32_t time){
 originalHud(self,dt,time);if(!self->uiMovie)return;
 Display data;ClickFeedback feedback;std::shared_ptr<const TerrainDisplay> terrain;std::vector<scape::Vec> route;{std::lock_guard lock(displayMutex);data=display;feedback=clickFeedback;terrain=terrainDisplay;route=displayedRoute;}
 RE::GFxValue root,clip;if(!self->uiMovie->GetVariable(&root,"_root"))return;
 drawTerrain(root,data.enabled&&data.overlay,terrain,route,self->uiMovie->GetVisibleFrameRect());
 auto combatCamera=RE::PlayerCamera::GetSingleton();
 combat_text::draw(root,combatCamera?findCamera(combatCamera->cameraRoot.get()):nullptr,self->uiMovie->GetVisibleFrameRect(),data.enabled);
 if(!root.GetMember("SkyrimScapePointer",&clip)||!clip.IsDisplayObject())if(!root.CreateEmptyMovieClip(&clip,"SkyrimScapePointer",16000))return;
 RE::GFxValue marker;
 if(!root.GetMember("SkyrimScapeClick",&marker)||!marker.IsDisplayObject())root.CreateEmptyMovieClip(&marker,"SkyrimScapeClick",15999);
 auto animation=scape::clickAnimation(std::chrono::duration<float>(Clock::now()-feedback.when).count());
 RE::GFxValue::DisplayInfo markerInfo;markerInfo.SetVisible(data.enabled&&animation.visible);
 if(marker.IsDisplayObject())marker.SetDisplayInfo(markerInfo);
 RE::GFxValue::DisplayInfo info;info.SetVisible(data.enabled);clip.SetDisplayInfo(info);if(!data.enabled)return;
 auto rect=self->uiMovie->GetVisibleFrameRect();
 if(marker.IsDisplayObject()&&animation.visible){
  markerInfo.SetPosition(rect.left+feedback.x*(rect.right-rect.left),rect.top+feedback.y*(rect.bottom-rect.top));
  markerInfo.SetAlpha(animation.alpha);marker.SetDisplayInfo(markerInfo);marker.Invoke("clear");
  const std::array<RE::GFxValue,3> stroke{RE::GFxValue(1.25),RE::GFxValue(feedback.action?16733525.:16770720.),RE::GFxValue(100.)};marker.Invoke("lineStyle",stroke);
  // Four outward-moving diagonal arms keep the clicked point visible at the center.
  for(int sx:{-1,1})for(int sy:{-1,1}){
   const std::array<RE::GFxValue,2> inner{RE::GFxValue(sx*animation.radius*.4),RE::GFxValue(sy*animation.radius*.4)};
   const std::array<RE::GFxValue,2> outer{RE::GFxValue(sx*animation.radius*1.),RE::GFxValue(sy*animation.radius*1.)};
   marker.Invoke("moveTo",inner);marker.Invoke("lineTo",outer);
  }
 }
 info.SetPosition(rect.left+data.x*(rect.right-rect.left),rect.top+data.y*(rect.bottom-rect.top));clip.SetDisplayInfo(info);
 clip.Invoke("clear");
 const std::array<RE::GFxValue,3> line{RE::GFxValue(2.),RE::GFxValue(16770720.),RE::GFxValue(100.)};clip.Invoke("lineStyle",line);
 const std::array<RE::GFxValue,2> a{RE::GFxValue(0.),RE::GFxValue(0.)},b{RE::GFxValue(0.),RE::GFxValue(17.)},c{RE::GFxValue(5.),RE::GFxValue(11.)},d{RE::GFxValue(12.),RE::GFxValue(11.)};
 clip.Invoke("moveTo",a);clip.Invoke("lineTo",b);clip.Invoke("lineTo",c);clip.Invoke("lineTo",d);clip.Invoke("lineTo",a);
}
void message(SKSE::MessagingInterface::Message* e){
 if(e->type==SKSE::MessagingInterface::kDataLoaded){
  RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESHitEvent>(&combat_text::hits);
  REL::Relocation<std::uintptr_t> controls{RE::VTABLE_PlayerControls[0]};originalInput=controls.write_vfunc(1,input);
  REL::Relocation<std::uintptr_t> camera{RE::VTABLE_ThirdPersonState[0]};originalRotation=camera.write_vfunc(4,rotation);originalTranslation=camera.write_vfunc(5,translation);
  originalCameraUpdate=camera.write_vfunc(3,cameraUpdate);
  REL::Relocation<std::uintptr_t> menu{RE::VTABLE_HUDMenu[0]};originalHud=menu.write_vfunc(5,hud);
  spdlog::info("Input, camera and HUD hooks installed; F8 enables prototype");
 }
 if(e->type==SKSE::MessagingInterface::kPreLoadGame||e->type==SKSE::MessagingInterface::kNewGame){combat_text::update(false);cancel(RE::PlayerControls::GetSingleton());state.enabled=false;state.middle=false;}
}
}
SKSEPluginLoad(const SKSE::LoadInterface* skse){
 if(skse->RuntimeVersion()!=REL::Version{1,7,104,0})return false;
 SKSE::Init(skse);auto directory=SKSE::log::log_directory();if(!directory)return false;
 auto log=spdlog::basic_logger_mt("SkyrimScape",(*directory/"SkyrimScape.log").string(),true);
 spdlog::set_default_logger(log);spdlog::flush_on(spdlog::level::info);
 spdlog::info("SkyrimScape experimental 0.3.4 loaded on {}",skse->RuntimeVersion().string());
 return SKSE::GetMessagingInterface()->RegisterListener(message);
}
