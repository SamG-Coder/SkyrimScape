#include "roof_policy.hpp"
#include <iostream>
int main(){
 using namespace scape;using namespace scape::roof;
 if(!named("Architecture\\FarmhouseRoof01.dds")||!named("CEILING:0")||!named("WoodRafter02"))return 1;
 if(named("FloorStone01")||named("WallPlaster"))return 2;
 Vec focus{0,0,75},camera{0,-500,600};
 if(!obstructs({0,-200,300},100,focus,camera))return 3;
 if(obstructs({600,-200,300},100,focus,camera))return 4;
 if(obstructs({0,0,0},100,focus,camera))return 5;
 if(obstructs({0,-900,1100},50,focus,camera))return 6;
 if(obstructs({0,-200,300},2000,focus,camera))return 7;
 if(obstructs({0,-200,300},100,focus,{0,-500,75}))return 8;
 if(!aboveInteriorCut({0,0,300},30,0))return 9;
 if(aboveInteriorCut({0,0,300},30,120))return 10; // Climbing reveals upper landing.
 if(aboveInteriorCut({0,0,200},60,0))return 11; // Keep pieces crossing the cutoff.
 if(aboveInteriorCut({0,0,-100},20,120))return 12; // Lower level stays visible.
 if(aboveInteriorCut({0,0,300},-1,0))return 13;
 std::cout<<"PASS: roof naming, camera obstruction, off-axis/floor/behind-camera/oversized bounds exclusions\n";
}
