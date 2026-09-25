#include "combat_policy.hpp"
#include <cstdlib>
#include <iostream>
void require(bool p,const char* text){if(!p){std::cerr<<text<<'\n';std::exit(1);}}
int main(){
 using namespace scape::combat;
 require(!inMeleeRange({0,0,0},{20,0,120},173),"Do not stop under an enemy on an upper floor");
 require(!inMeleeRange({0,0,0},{170,0,60},173),"Stair height must count toward weapon distance");
 require(inMeleeRange({0,0,0},{140,0,40},173),"Nearby stair opponent should remain attackable");
 auto shortWeapon=meleeRange(70),longWeapon=meleeRange(140);
 require(shortWeapon.strike<longWeapon.strike&&shortWeapon.approach<longWeapon.approach,"Longer weapon must increase attack and approach ranges");
 require(shortWeapon.strike<70&&longWeapon.strike<140,"Attack range must stay inside native reach");
 require(shortWeapon.approach+18<shortWeapon.strike&&longWeapon.approach+18<longWeapon.strike,"Route arrival tolerance must stay within strike range");
 require(meleeRange(NAN).strike==meleeRange(64).strike&&meleeRange(-1).strike==meleeRange(64).strike&&meleeRange(INFINITY).strike==meleeRange(64).strike,"Invalid reach must use bounded fallback");
 Motion rangeMotion;rangeMotion.update({0,150,0},1);rangeMotion.update({20,150,0},1.1);
 require(rangeMotion.intercept({0,0,0},{20,150,0},160).x==20&&rangeMotion.intercept({0,0,0},{20,150,0},60).x>20,"Long weapon can stop leading while short weapon keeps chasing");
 require(lightHoldSeconds(.5f)==.12f,"Normal attack needs a sustained short press");
 require(lightHoldSeconds(.15f)<.15f&&lightHoldSeconds(.1f)<.1f,"Light hold must stay below power threshold");
 require(lightHoldSeconds(0.f)==.12f,"Invalid power delay must use safe default");
 require(rangedChoice(160,600,.8f,.8f)==RangedChoice::closeShooter,"Nearby ranger should be engaged when healthy");
 require(rangedChoice(900,100,.8f,.8f)==RangedChoice::keepFighting,"One distant arrow must not abandon an immediate melee fight");
 require(rangedChoice(900,600,.8f,.8f)==RangedChoice::seekCover,"Long exposed chase should search for cover");
 require(rangedChoice(400,600,.3f,.8f)==RangedChoice::seekCover&&rangedChoice(400,600,.8f,.1f)==RangedChoice::seekCover,"Low health or stamina should favour cover");
 require(rangedChoice(350,600,.8f,.8f)==RangedChoice::closeShooter,"Healthy player can close a moderate gap");
 require(shouldBlock(true,true,false,25,100),"Healthy stamina must permit defense");
 require(!shouldBlock(true,true,false,15,100)&&shouldBlock(true,true,true,15,100),"Block hysteresis must avoid repeated press/release at threshold");
 require(!shouldBlock(true,true,true,9,100),"Low stamina must release the block");
 require(!shouldBlock(false,true,true,100,100)&&!shouldBlock(true,false,false,100,100),"No threat or incompatible equipment must not block");
 require(!shouldBlock(true,true,false,30,200),"Defense must scale with maximum stamina");
 require(shouldPower(false,true,true,3,80,100),"An opening after several attacks with reserve stamina permits a heavy attack");
 require(!shouldPower(true,true,true,3,80,100),"Incoming attack must suppress a new heavy attack");
 require(!shouldPower(false,true,false,3,80,100)&&!shouldPower(false,true,true,2,80,100),"Cooldown and cadence must prevent heavy-attack spam");
 require(!shouldPower(false,true,true,3,40,100)&&!shouldPower(false,true,true,3,80,200),"Heavy attack must retain a stamina reserve");
 require(!shouldPower(false,false,true,3,100,100),"Melee automation must not charge ranged weapons");
 Motion motion;motion.update({0,400,0},1);motion.update({20,400,0},1.1);
 auto lead=motion.intercept({0,0,0},{20,400,0});
 require(lead.x>20&&lead.y==400&&scape::planarDistance(lead,{20,400,0})<=120,"Chase must lead observed lateral movement with a bounded horizon");
 require(motion.intercept({20,350,0},{20,400,0}).x==20,"Close melee aiming must use the actual target position");
 motion.update({900,400,0},1.2);require(motion.velocity.length()==0,"Teleport must clear prediction");
 motion.update({910,400,0},4);require(motion.velocity.length()==0,"Stale velocity must not survive a pause");
 require(shouldEvade(true,false,true,true,15),"Free grounded player can evade when unable to block");
 require(!shouldEvade(true,true,true,true,100)&&!shouldEvade(true,false,false,true,100)&&!shouldEvade(true,false,true,false,100),"Defense, committed attacks and airborne traversal must suppress sidesteps");
 require(shouldRetarget(true,100,400)&&!shouldRetarget(true,100,110)&&!shouldRetarget(false,100,400)&&!shouldRetarget(true,400,500),"Only a recent nearby hostile attacker may interrupt a distant chase");
 std::cout<<"PASS: combat stamina reserve, defense hysteresis, threat priority and power cadence\n";
}
