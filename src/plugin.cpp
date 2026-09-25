#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <chrono>
#include <mutex>
#include "control_math.hpp"
#include "native_navigation.hpp"
#include "terrain_overlay.hpp"
#include "combat_text.hpp"
#include "combat_policy.hpp"
#include "route_following.hpp"
#include "context_menu.hpp"
namespace {
using Clock=std::chrono::steady_clock;
scape::Vec vec(RE::NiPoint3 p){return {p.x,p.y,p.z};}
RE::NiPoint3 point(scape::Vec p){return {p.x,p.y,p.z};}
enum class Order{none,walk,interact,attack};
struct State{
 bool enabled{},middle{},ownsMovement{},attackHeld{};
 bool blockHeld{},powerHeld{};unsigned lightAttacks{};
 Clock::time_point attackStarted{},attackRelease{},blockStarted{},blockUntil{},nextPower{},nextBlock{};
 bool overlay{true};Clock::time_point nextOverlay{};
 scape::Orbit orbit;
 float cursorX{.5f},cursorY{.5f};
 Order order{Order::none};
 bool talkOnly{};
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
 scape::Vec planOrigin{};Clock::time_point nextLookahead{};
 Clock::time_point planStarted{};std::size_t planSlices{};double maxPlanSliceMs{};
 Clock::time_point nextMovementLog{};
 bool savedRunning{},runningOwned{};
 Clock::time_point nextTerrainCheck{},nextJump{};
}state;
struct ContextMenu{
 bool open{};std::uint32_t revision{};scape::menu::Layout layout;
 std::string title;std::vector<scape::menu::Row> rows;RE::ObjectRefHandle target;
 scape::Vec ground;RE::FormID world{},cell{};
}contextMenu,displayedMenu;
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
void attackButton(RE::PlayerControls* controls,bool down,float held=0.f){
 if(!controls||!controls->attackBlockHandler)return;
 auto e=RE::ButtonEvent::Create(RE::INPUT_DEVICE::kMouse,RE::UserEvents::GetSingleton()->rightAttack,0,down?1.f:0.f,down?held:(std::max)(.1f,held));
 if(e){controls->attackBlockHandler->ProcessButton(e,&controls->data);delete e;}
 state.attackHeld=down;
}
void blockButton(RE::PlayerControls* controls,bool down,float held=0.f){
 if(!controls||!controls->attackBlockHandler)return;
 auto e=RE::ButtonEvent::Create(RE::INPUT_DEVICE::kMouse,RE::UserEvents::GetSingleton()->leftAttack,1,down?1.f:0.f,down?held:(std::max)(.1f,held));
 if(e){controls->attackBlockHandler->ProcessButton(e,&controls->data);delete e;}state.blockHeld=down;
}
void cancel(RE::PlayerControls* c){
 contextMenu.open=false;
 native_navigation::gridCache().grid.abandon();state.planning=false;
 if(state.attackHeld)attackButton(c,false);
 if(state.blockHeld)blockButton(c,false);
 state.powerHeld=false;state.blockUntil={};
 if(c&&state.ownsMovement){c->data.moveInputVec={0,0};c->data.prevMoveVec={0,0};state.ownsMovement=false;}
 if(c&&state.runningOwned){c->data.running=state.savedRunning;state.runningOwned=false;}
 state.order=Order::none;state.target={};
 state.talkOnly=false;
 state.route.clear();state.waypoint=0;
 state.traversal.clear();state.activeTraversal=(std::numeric_limits<std::size_t>::max)();
}
struct Hit{bool valid{};scape::Vec position;RE::TESObjectREFR* reference{};float normalZ{};};
Hit cast(RE::NiPoint3 start,RE::NiPoint3 end,bool logIgnored=false,bool groundOnly=false){
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
 if(ignored(firstRef)||groundOnly){
  RE::hkpAllRayHitCollector collector;RE::bhkPickData all{};all.rayInput=pick.rayInput;all.allRayHitCollector=&collector;world->PickObject(all);
  const RE::hkpWorldRayCastOutput* nearest=nullptr;
  for(const auto& hit:collector.hits){
   if(!hit.HasHit()||!hit.rootCollidable)continue;
   if(ignored(RE::TESHavokUtilities::FindCollidableRef(*hit.rootCollidable)))continue;
   if(groundOnly){auto ref=RE::TESHavokUtilities::FindCollidableRef(*hit.rootCollidable);if(ref&&ref->As<RE::Actor>())continue;alignas(16) float n[4];_mm_store_ps(n,hit.normal.quad);if(n[2]<.55f)continue;}
   if(!nearest||hit.hitFraction<nearest->hitFraction)nearest=&hit;
  }
  if(logIgnored)spdlog::info("PICK filtered reference {:08X}; groundOnly={} remaining hit={}",firstRef?firstRef->GetFormID():0,groundOnly,nearest!=nullptr);
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
 if(!state.planning){state.planStarted=started;state.planSlices=0;state.maxPlanSliceMs=0;state.planOrigin=position;}
 auto goalVisible=[](scape::Vec approach){
  auto target=state.target.get();if(!target)return false;
  auto hit=cast(point(approach+scape::Vec{0,0,60}),point(state.destination+scape::Vec{0,0,60}));
  return !hit.valid||hit.reference==target.get();
 };
 auto result=native_navigation::plan(state.planOrigin,state.destination,state.order==Order::walk,clearTraversal,goalVisible);
 ++state.planSlices;auto elapsed=std::chrono::duration<double,std::milli>(Clock::now()-started).count();state.maxPlanSliceMs=(std::max)(state.maxPlanSliceMs,elapsed);
 if(elapsed>12)spdlog::info("PLAN slow slice {:.2f} ms expanded={} pending={}",elapsed,result.stats.expanded,result.pending);
 state.planning=result.pending;state.nextPlan=Clock::now()+std::chrono::milliseconds(result.pending?0:750);
 if(result.pending)return false;
 auto& route=result.route;
 if(route.points.empty()){spdlog::info("No usable grid route: slices={} maxSliceMs={:.2f}; see preceding GRID reason",state.planSlices,state.maxPlanSliceMs);return false;}
 if(scape::planarDistance(position,state.planOrigin)>8.f&&!scape::joinMovingRoute(route,position,[](scape::Vec a,scape::Vec b){return native_navigation::gridCache().grid.canWalk(a,b,clearTraversal);})){spdlog::info("PLAN moving join blocked; keep current validated route");return false;}
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
void issueHit(Hit hit,std::optional<Order> forced={}){
 auto p=RE::PlayerCharacter::GetSingleton();
 bool actionHit=forced?*forced!=Order::walk:hit.reference&&hit.reference->GetBaseObject()&&(hit.reference->As<RE::Actor>()||hit.reference->GetBaseObject()->IsInventoryObject()||hit.reference->GetBaseObject()->GetFormType()==RE::FormType::Door||hit.reference->GetBaseObject()->GetFormType()==RE::FormType::Container||hit.reference->GetBaseObject()->GetFormType()==RE::FormType::Activator||hit.reference->GetBaseObject()->GetFormType()==RE::FormType::Furniture);
 const auto previous=state;
 bool keepWalking=!actionHit&&state.order==Order::walk&&state.waypoint<state.route.size()&&
  state.activeTraversal==(std::numeric_limits<std::size_t>::max)()&&scape::sameWalkDirection(vec(p->GetPosition()),state.route[state.waypoint],hit.position);
 if(keepWalking){native_navigation::gridCache().grid.abandon();state.planning=false;spdlog::info("CLICK keeps active walk while replacing destination");}
 else cancel(RE::PlayerControls::GetSingleton());
 state.destination=hit.position;state.order=Order::walk;
 if(forced){state.order=*forced;if(state.order!=Order::walk){state.target=hit.reference->GetHandle();state.destination=vec(hit.reference->GetPosition());}}
 else if(hit.reference&&hit.reference->GetBaseObject()){
  auto actor=hit.reference->As<RE::Actor>();auto type=hit.reference->GetBaseObject()->GetFormType();
  if(actor&&!actor->IsDead()&&actor->IsHostileToActor(p))state.order=Order::attack;
  else if(actor||type==RE::FormType::Door||type==RE::FormType::Container||type==RE::FormType::Activator||type==RE::FormType::Furniture||hit.reference->GetBaseObject()->IsInventoryObject())state.order=Order::interact;
  if(state.order!=Order::walk){state.target=hit.reference->GetHandle();state.destination=vec(hit.reference->GetPosition());}
 }
 state.lastPosition=vec(p->GetPosition());state.lastProgress=Clock::now();state.nextAttack={};
 if(state.order==Order::walk||!actionInReach(p,hit.reference)){
  if(!planRoute(state.lastPosition)&&!state.planning&&state.order==Order::walk){rejectedClick();cancel(RE::PlayerControls::GetSingleton());if(previous.order==Order::walk){state=previous;state.planning=false;}return;}
 }else{state.plannedTarget=state.destination;state.nextPlan={};}
 if(state.order==Order::walk&&!state.planning&&!state.route.empty())state.destination=state.route.back();
 spdlog::info("Click screen {:.3f} {:.3f}, collision {:.1f} {:.1f} {:.1f}, selected {:.1f} {:.1f} {:.1f}",state.cursorX,state.cursorY,hit.position.x,hit.position.y,hit.position.z,state.destination.x,state.destination.y,state.destination.z);
 {std::lock_guard lock(displayMutex);clickFeedback={state.cursorX,state.cursorY,state.order!=Order::walk,Clock::now()};}
 spdlog::info("Order {} target {:08X} destination {:.1f} {:.1f} {:.1f}",static_cast<int>(state.order),hit.reference?hit.reference->GetFormID():0,state.destination.x,state.destination.y,state.destination.z);
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
 // Back-facing/vertical geometry is not a walking destination. Follow the same
 // screen ray to an upward-facing surface instead of dropping its XYZ to ground.
 bool actionHit=hit.reference&&hit.reference->As<RE::Actor>();
 if(hit.reference&&hit.reference->GetBaseObject()){
  auto base=hit.reference->GetBaseObject();auto type=base->GetFormType();
  actionHit=actionHit||base->IsInventoryObject()||type==RE::FormType::Door||type==RE::FormType::Container||type==RE::FormType::Activator||type==RE::FormType::Furniture;
 }
 if(!actionHit&&hit.normalZ<.55f){
  hit=cast(start,start+direction*12000.f,true,true);
  if(!hit.valid){rejectedClick();return;}
  spdlog::info("PICK ground ray continued to {:.2f} {:.2f} {:.2f} normalZ={:.3f}",hit.position.x,hit.position.y,hit.position.z,hit.normalZ);
 }
 issueHit(hit);
}

void openContextMenu(){
 if(contextMenu.open){contextMenu.open=false;return;}
 auto pc=RE::PlayerCamera::GetSingleton();auto camera=pc?findCamera(pc->cameraRoot.get()):nullptr;if(!camera)return;
 RE::NiPoint3 start,direction;if(!camera->WindowPointToRay(static_cast<int>(state.cursorX*10000),static_cast<int>(state.cursorY*10000),start,direction,10000,10000))return;
 auto hit=cast(start,start+direction*12000.f,true);auto ground=cast(start,start+direction*12000.f,false,true);
 ContextMenu menu;menu.revision=contextMenu.revision+1;menu.title="Choose action";menu.open=true;
 auto player=RE::PlayerCharacter::GetSingleton();menu.world=player->GetWorldspace()?player->GetWorldspace()->GetFormID():0;menu.cell=player->GetParentCell()?player->GetParentCell()->GetFormID():0;
 if(hit.valid&&hit.reference&&hit.reference!=player&&hit.reference->GetBaseObject()){
  auto ref=hit.reference;menu.target=ref->GetHandle();auto type=ref->GetBaseObject()->GetFormType();
  if(auto name=ref->GetDisplayFullName();name&&*name){menu.title=name;if(menu.title.size()>32)menu.title=menu.title.substr(0,29)+"...";}
  if(auto actor=ref->As<RE::Actor>())menu.rows=scape::menu::actorRows(actor->IsDead(),actor->IsHostileToActor(player),actor->CanTalkToPlayer());
  else if(type==RE::FormType::Door||type==RE::FormType::Container)menu.rows.push_back({scape::menu::Action::activate,"Open"});
  else if(ref->GetBaseObject()->IsInventoryObject())menu.rows.push_back({scape::menu::Action::activate,"Take"});
  else if(type==RE::FormType::Furniture)menu.rows.push_back({scape::menu::Action::activate,"Use"});
  else if(type==RE::FormType::Activator)menu.rows.push_back({scape::menu::Action::activate,"Activate"});
 }
 if(ground.valid){auto floor=scape::nav::surface(native_navigation::refreshGrid().mesh,ground.position,48.f,64.f,40.f);
  if(floor){menu.ground=floor->point;menu.rows.push_back({scape::menu::Action::walk,"Walk here"});}
 }
 menu.rows.push_back({scape::menu::Action::stop,"Stop"});menu.rows.push_back({scape::menu::Action::close,"Cancel"});
 menu.layout.rows=menu.rows.size();menu.layout.place(state.cursorX,state.cursorY);
 cancel(RE::PlayerControls::GetSingleton());contextMenu=std::move(menu);
 spdlog::info("CONTEXT opened target={:08X} actions={}",hit.reference?hit.reference->GetFormID():0,contextMenu.rows.size());
}
void selectContextMenu(){
 auto menu=contextMenu;contextMenu.open=false;auto index=menu.layout.hit(state.cursorX,state.cursorY);
 if(index<0)return;auto row=menu.rows[static_cast<std::size_t>(index)];
 if(row.action==scape::menu::Action::close)return;
 if(row.action==scape::menu::Action::stop){cancel(RE::PlayerControls::GetSingleton());return;}
 auto player=RE::PlayerCharacter::GetSingleton();auto world=player->GetWorldspace();auto cell=player->GetParentCell();
 if((world?world->GetFormID():0)!=menu.world||(!world&&(!cell||cell->GetFormID()!=menu.cell))){rejectedClick();return;}
 if(row.action==scape::menu::Action::walk){issueHit({true,menu.ground,nullptr,1.f},Order::walk);return;}
 auto target=menu.target.get();if(!target||!target->Get3D()||target->GetWorldspace()!=world||(!world&&target->GetParentCell()!=cell)){rejectedClick();return;}
 auto actor=target->As<RE::Actor>();
 if(row.action==scape::menu::Action::attack&&(!actor||actor->IsDead())){rejectedClick();return;}
 if(row.action==scape::menu::Action::talk&&(!actor||actor->IsDead()||actor->IsHostileToActor(player)||!actor->CanTalkToPlayer())){rejectedClick();return;}
 spdlog::info("CONTEXT selected {} target={:08X}",row.label,target->GetFormID());
 issueHit({true,vec(target->GetPosition()),target.get(),1.f},row.action==scape::menu::Action::attack?Order::attack:Order::interact);
 state.talkOnly=row.action==scape::menu::Action::talk;
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
 if(state.attackHeld){
  float held=std::chrono::duration<float>(Clock::now()-state.attackStarted).count();
  if(!state.powerHeld||Clock::now()>=state.attackRelease){attackButton(c,false,held);state.powerHeld=false;}
  else attackButton(c,true,held);
 }
 if(state.order==Order::none)return;
 auto now=Clock::now();auto position=vec(p->GetPosition());RE::NiPointer<RE::TESObjectREFR> target;
 if(state.order!=Order::walk){
  target=state.target.get();
  if(!target||!target->Get3D()||target->GetWorldspace()!=world||(!world&&target->GetParentCell()!=cell)){cancel(c);return;}
  state.destination=vec(target->GetPosition());
  if(state.talkOnly){auto actor=target->As<RE::Actor>();if(!actor||actor->IsDead()||actor->IsHostileToActor(p)||!actor->CanTalkToPlayer()){cancel(c);return;}}
  if(state.order==Order::attack){auto actor=target->As<RE::Actor>();if(!actor||actor->IsDead()){spdlog::info("Attack order ended: target is dead or no longer an actor");cancel(c);return;}}
 }
 bool incoming=false;
 if(state.order==Order::attack){
  auto actor=target->As<RE::Actor>();auto attack=actor->GetAttackState();
  float facing=std::remainder(actor->GetAngleZ()-scape::heading(state.destination,position),6.2831853f);
  bool visible=false;
  incoming=(attack==RE::ATTACK_STATE_ENUM::kDraw||attack==RE::ATTACK_STATE_ENUM::kSwing||attack==RE::ATTACK_STATE_ENUM::kHit||attack==RE::ATTACK_STATE_ENUM::kBash)&&
   scape::planarDistance(position,state.destination)<180.f&&std::abs(position.z-state.destination.z)<90.f&&std::abs(facing)<1.15f&&p->HasLineOfSight(target.get(),visible);
  auto left=p->GetEquippedObject(true),right=p->GetEquippedObject(false);
  auto weapon=right?right->As<RE::TESObjectWEAP>():nullptr;
  auto armor=left?left->As<RE::TESObjectARMO>():nullptr;
  bool capable=RE::ControlMap::GetSingleton()->IsFightingControlsEnabled()&&p->IsWeaponDrawn()&&((armor&&armor->IsShield())||((!left||left==right)&&weapon&&weapon->IsMelee()));
  if(incoming)state.blockUntil=now+std::chrono::milliseconds(180);
  float stamina=p->GetActorValue(RE::ActorValue::kStamina),maximum=p->GetPermanentActorValue(RE::ActorValue::kStamina);
  bool defend=scape::combat::shouldBlock(incoming||(state.blockHeld&&now<state.blockUntil),capable,state.blockHeld,stamina,maximum)&&
   !state.attackHeld&&!p->IsStaggered()&&p->GetAttackState()==RE::ATTACK_STATE_ENUM::kNone&&now>=state.nextBlock;
  if(state.blockHeld&&now-state.blockStarted>std::chrono::milliseconds(1200)){defend=false;state.nextBlock=now+std::chrono::milliseconds(300);}
  if(defend){
   if(!state.blockHeld){state.blockStarted=now;spdlog::info("AUTO BLOCK target={:08X} stamina={:.1f}",actor->GetFormID(),stamina);}
   p->SetHeading(scape::heading(position,state.destination));
   blockButton(c,true,std::chrono::duration<float>(now-state.blockStarted).count());
   c->data.moveInputVec={0,0};c->data.prevMoveVec={0,0};state.lastProgress=now;
   state.nextAttack=now+std::chrono::milliseconds(250);return;
  }
 }
 if(state.blockHeld){blockButton(c,false);spdlog::info("AUTO BLOCK released");}
 bool pendingTraversal=false;for(auto i=state.waypoint;i<state.traversal.size();++i)if(state.traversal[i]!=scape::nav::Traversal::walk)pendingTraversal=true;
 const bool arrived=!state.planning&&!pendingTraversal&&state.activeTraversal==(std::numeric_limits<std::size_t>::max)()&&
  (state.order==Order::walk?(scape::reached(position,state.destination,orderRange())&&std::abs(position.z-state.destination.z)<=35.f):actionInReach(p,target.get()));
 if(!arrived){
  if((state.planning||state.order!=Order::walk)&&state.activeTraversal==(std::numeric_limits<std::size_t>::max)()&&now>=state.nextPlan&&
     (state.planning||state.route.empty()||state.waypoint>=state.route.size()||(state.destination-state.plannedTarget).length()>80.f)){
   if(!planRoute(position)){
    bool keep=state.order==Order::walk&&state.waypoint<state.route.size();
    if(!keep){
     c->data.moveInputVec={0,0};c->data.prevMoveVec={0,0};
     state.route.clear();state.traversal.clear();state.waypoint=0;state.lastProgress=now;
     if(!state.planning&&state.order==Order::walk){rejectedClick();cancel(c);}return;
    }
    if(!state.planning){rejectedClick();state.destination=state.route.back();spdlog::info("PLAN replacement rejected; continuing previous walk");}
   }
   if(state.order==Order::walk&&!state.planning&&!state.route.empty())state.destination=state.route.back();
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
   if(state.order==Order::walk&&!state.planning)cancel(c);
   return;
  }
  if(now>=state.nextLookahead&&state.activeTraversal==(std::numeric_limits<std::size_t>::max)()){
   state.nextLookahead=now+std::chrono::milliseconds(100);
   auto end=(std::min)(state.route.size(),state.waypoint+3);
   for(auto next=state.waypoint+1;next<end;++next){
    if(state.traversal[next-1]!=scape::nav::Traversal::walk||state.traversal[next]!=scape::nav::Traversal::walk)break;
    if(!native_navigation::gridCache().grid.canWalk(position,state.route[next],clearTraversal))break;
    state.waypoint=next;
   }
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
   const float speed=std::clamp(scape::planarDistance(position,state.route.back())/100.f,.3f,1.f);
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
 if(now>=state.nextAttack&&!state.attackHeld&&p->GetAttackState()==RE::ATTACK_STATE_ENUM::kNone&&!p->IsStaggered()){
  auto actor=target->As<RE::Actor>();
  auto right=p->GetEquippedObject(false);auto weapon=right?right->As<RE::TESObjectWEAP>():nullptr;
  float stamina=p->GetActorValue(RE::ActorValue::kStamina),maximum=p->GetPermanentActorValue(RE::ActorValue::kStamina);
  bool power=scape::combat::shouldPower(incoming,weapon&&weapon->IsMelee(),now>=state.nextPower,state.lightAttacks,stamina,maximum);
  spdlog::info("Melee input target {:08X}, range {:.1f}, target health {:.1f}",target->GetFormID(),scape::planarDistance(position,state.destination),actor?actor->GetActorValue(RE::ActorValue::kHealth):0.f);
  state.attackStarted=now;state.powerHeld=power;
  if(power){
   float delay=.5f;if(auto settings=RE::GameSettingCollection::GetSingleton())if(auto setting=settings->GetSetting("fPowerAttackDelay")){auto value=setting->GetFloat();if(std::isfinite(value)&&value>.1f&&value<2.f)delay=value;}
   state.attackRelease=now+std::chrono::milliseconds(static_cast<int>((delay+.2f)*1000));state.nextPower=now+std::chrono::seconds(6);state.lightAttacks=0;
   spdlog::info("AUTO POWER target={:08X} stamina={:.1f} holdSeconds={:.2f}",actor->GetFormID(),stamina,delay+.2f);
  }else ++state.lightAttacks;
  attackButton(c,true);state.nextAttack=now+std::chrono::milliseconds(power?1600:1000);
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
    consume=true;if(id==2){state.middle=b->IsPressed();if(state.middle)contextMenu.open=false;}
    if(b->IsDown()){if(id==0){if(contextMenu.open)selectContextMenu();else click();}if(id==1)openContextMenu();}
    if(auto steps=scape::wheelStep(id,b->Value());steps!=0){
     state.orbit.zoom(steps);
     spdlog::info("Mouse wheel {} -> camera distance {:.1f}",steps>0?"up/in":"down/out",state.orbit.distance);
    }
   }
   if(active&&e->GetDevice()==RE::INPUT_DEVICE::kKeyboard&&b->IsDown()){
    if(id==0x01&&contextMenu.open){contextMenu.open=false;consume=true;}
    else if(id==0x11||id==0x1E||id==0x1F||id==0x20||id==0x01)cancel(c);
   }
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
 {std::lock_guard lock(displayMutex);display={active,state.cursorX,state.cursorY,state.overlay};displayedMenu=contextMenu;displayedRoute.clear();
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
  root.CreateEmptyMovieClip(&label,"SkyrimScapeTerrainLegend",15998);
  system_text::draw(label,"F7: GRID | GREEN: SUPPORTED CELLS\nWHITE: ROUTE | X-RAY VIEW",2.,0xFFFFFF);
  RE::GFxValue::DisplayInfo placement;placement.SetPosition(14,45);label.SetDisplayInfo(placement);
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
void drawContextMenu(RE::GFxValue& root,const ContextMenu& menu,const Display& cursor,const RE::GRectF& rect){
 RE::GFxValue panel;
 if(!root.GetMember("SkyrimScapeContext",&panel)||!panel.IsDisplayObject()){
  if(!menu.open)return;if(!root.CreateEmptyMovieClip(&panel,"SkyrimScapeContext",16500))return;
 }
 RE::GFxValue::DisplayInfo info;info.SetVisible(menu.open&&cursor.enabled);panel.SetDisplayInfo(info);if(!menu.open||!cursor.enabled)return;
 auto w=rect.right-rect.left,h=rect.bottom-rect.top;double width=menu.layout.width*w,height=menu.layout.height()*h,header=menu.layout.header*h,rowHeight=menu.layout.rowHeight*h;
 info.SetPosition(rect.left+menu.layout.x*w,rect.top+menu.layout.y*h);panel.SetDisplayInfo(info);
 panel.Invoke("clear");
 auto box=[&](double x,double y,double bw,double bh,double color,double alpha){
  const std::array<RE::GFxValue,2> fill{RE::GFxValue(color),RE::GFxValue(alpha)};panel.Invoke("beginFill",fill);
  auto vertex=[&](const char* method,double a,double b){const std::array<RE::GFxValue,2> p{RE::GFxValue(a),RE::GFxValue(b)};panel.Invoke(method,p);};
  vertex("moveTo",x,y);vertex("lineTo",x+bw,y);vertex("lineTo",x+bw,y+bh);vertex("lineTo",x,y+bh);vertex("lineTo",x,y);panel.Invoke("endFill");
 };
 box(4,5,width,height,0,45);box(0,0,width,height,0x9C895B,100);box(1,1,width-2,height-2,0x141A20,97);
 box(1,1,width-2,header-2,0x222B34,100);box(10,header-2,width-20,1,0x9C895B,65);
 auto hover=menu.layout.hit(cursor.x,cursor.y);
 if(hover>=0){box(4,header+hover*rowHeight,width-8,rowHeight,0x3A4653,100);box(4,header+hover*rowHeight,3,rowHeight,0xEAC477,100);}
 RE::GFxValue oldRevision;bool rebuild=!panel.GetMember("revision",&oldRevision)||!oldRevision.IsNumber()||oldRevision.GetNumber()!=menu.revision;
 auto text=[&](const std::string& name,std::string label,int depth,double y,double color){
  RE::GFxValue field;bool created=!panel.GetMember(name.c_str(),&field)||!field.IsDisplayObject();
  if(created&&!panel.CreateEmptyMovieClip(&field,name.c_str(),depth))return;
  if(rebuild||created){
   while(label.size()>3&&system_text::width(label,2.25)>width-28)label=label.substr(0,label.size()-4)+"...";
   system_text::draw(field,label,2.25,color);
  }
  RE::GFxValue::DisplayInfo placement;placement.SetPosition(14,y);placement.SetVisible(true);field.SetDisplayInfo(placement);
 };
 text("title",menu.title,1,(header-24)*.5,0xEAC477);
 for(std::size_t i=0;i<8;++i){
  auto name="row"+std::to_string(i);
  if(i<menu.rows.size()){auto& row=menu.rows[i];text(name,row.label,static_cast<int>(i+2),header+i*rowHeight+(rowHeight-24)*.5,row.action==scape::menu::Action::attack?0xFF8888:row.action==scape::menu::Action::walk?0xEAC477:0xE9EDF1);}
  else{RE::GFxValue old;if(panel.GetMember(name.c_str(),&old)&&old.IsDisplayObject()){RE::GFxValue::DisplayInfo hidden;hidden.SetVisible(false);old.SetDisplayInfo(hidden);}}
 }
 panel.SetMember("revision",RE::GFxValue(static_cast<double>(menu.revision)));
}
void hud(RE::HUDMenu* self,float dt,std::uint32_t time){
 originalHud(self,dt,time);if(!self->uiMovie)return;
 Display data;ContextMenu menu;ClickFeedback feedback;std::shared_ptr<const TerrainDisplay> terrain;std::vector<scape::Vec> route;{std::lock_guard lock(displayMutex);data=display;menu=displayedMenu;feedback=clickFeedback;terrain=terrainDisplay;route=displayedRoute;}
 RE::GFxValue root,clip;if(!self->uiMovie->GetVariable(&root,"_root"))return;
 // Use the HUD's own enable method; save its setting on this movie so a
 // recreated HUD or a user-disabled crosshair is restored correctly.
 RE::GFxValue base,saved;
 if(root.GetMember("HUDMovieBaseInstance",&base)&&base.IsDisplayObject()){
  bool owned=base.GetMember("SkyrimScapeSavedCrosshair",&saved)&&saved.IsBool();
  if(data.enabled){
   if(!owned){RE::GFxValue enabled;if(base.GetMember("bCrosshairEnabled",&enabled)&&enabled.IsBool()){base.SetMember("SkyrimScapeSavedCrosshair",enabled);owned=true;}}
   if(owned){const std::array<RE::GFxValue,1> args{RE::GFxValue(false)};base.Invoke("SetCrosshairEnabled",args);}
  }else if(owned){const std::array<RE::GFxValue,1> args{saved};base.Invoke("SetCrosshairEnabled",args);base.SetMember("SkyrimScapeSavedCrosshair",RE::GFxValue());}
 }
 drawTerrain(root,data.enabled&&data.overlay,terrain,route,self->uiMovie->GetVisibleFrameRect());
 auto combatCamera=RE::PlayerCamera::GetSingleton();
 combat_text::draw(root,combatCamera?findCamera(combatCamera->cameraRoot.get()):nullptr,self->uiMovie->GetVisibleFrameRect(),data.enabled);
 drawContextMenu(root,menu,data,self->uiMovie->GetVisibleFrameRect());
 if(!root.GetMember("SkyrimScapePointer",&clip)||!clip.IsDisplayObject())if(!root.CreateEmptyMovieClip(&clip,"SkyrimScapePointer",17000))return;
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
 spdlog::info("SkyrimScape experimental 0.3.11 loaded on {}",skse->RuntimeVersion().string());
 return SKSE::GetMessagingInterface()->RegisterListener(message);
}
