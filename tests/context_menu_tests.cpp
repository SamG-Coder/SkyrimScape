#include "context_menu.hpp"
#include <cstdlib>
#include <iostream>
void require(bool p,const char* message){if(!p){std::cerr<<message<<'\n';std::exit(1);}}
int main(){
 using namespace scape::menu;
 auto friendly=actorRows(false,false);require(friendly.size()==2&&friendly[0].action==Action::talk&&friendly[1].action==Action::attack,"Living non-hostile actors must offer talk and explicit attack");
 auto hostile=actorRows(false,true);require(hostile.size()==1&&hostile[0].action==Action::attack,"Hostile actors must not offer talk");
 auto dead=actorRows(true,false);require(dead.size()==1&&dead[0].action==Action::activate&&dead[0].label=="Search","Corpses must offer search rather than attack/talk");
 auto animal=actorRows(false,false,false);require(animal[0].action==Action::activate,"Actors without dialogue must not offer Talk");
 Layout layout;layout.rows=5;layout.place(.99f,.99f);
 require(layout.x+layout.width<=.991f&&layout.y+layout.height()<=.991f,"Menu must fit at lower-right screen edge");
 for(int i=0;i<5;++i)require(layout.hit(layout.x+.02f,layout.y+layout.header+layout.rowHeight*(i+.5f))==i,"Each visual row must map to its own action");
 require(layout.hit(layout.x+.02f,layout.y+.01f)==-1&&layout.hit(layout.x-.01f,layout.y+.1f)==-1&&layout.hit(layout.x+.02f,layout.y+layout.height())==-1,"Header and outside clicks must dismiss without selecting an action");
 Layout many;many.rows=30;many.place(.99f,.99f);
 require(many.visibleRows()==12&&many.y+many.height()<=.991f,"Long door menus must fit on screen");
 for(int i=0;i<40;++i)many.scroll(-1);
 require(many.offset==18&&many.hit(many.x+.01f,many.y+many.header+many.rowHeight*11.5f)==29,"Scrolled last row must select the last actual door");
 for(int i=0;i<40;++i)many.scroll(1);
 require(many.offset==0,"Scrolling above first door must clamp");
 Row a{Action::activate,"Open Door",123},b{Action::activate,"Open Door",456};
 require(a.target!=b.target,"Identically named doors must preserve distinct targets");
 std::cout<<"PASS: contextual actions, viewport placement and menu hit testing\n";
}
