#include "combat_policy.hpp"
#include <cstdlib>
#include <iostream>
void require(bool p,const char* text){if(!p){std::cerr<<text<<'\n';std::exit(1);}}
int main(){
 using namespace scape::combat;
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
 std::cout<<"PASS: combat stamina reserve, defense hysteresis, threat priority and power cadence\n";
}
