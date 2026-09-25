#include "system_glyphs.hpp"
#include <iostream>
int main(){
 system_text::Font font;
 if(font.faceName!=L"Segoe UI"){std::wcerr<<L"Unexpected font: "<<font.faceName;return 1;}
 bool antialiased=false;
 for(unsigned char c:std::string("0123456789.BLOCKF7: GRID | GREEN SUPPORTED CELLSWHITE ROUTEX-YAV")){
  auto& g=font.get(c);if(!g.valid||g.metrics.gmCellIncX<=0)return 2;
  if(c==' '&&(!g.pixels.empty()||g.metrics.gmBlackBoxX||g.metrics.gmBlackBoxY))return 6;
  if(c!=' '&&g.pixels.empty())return 3;
  // Exercise the renderer's exact row/column addressing, including whitespace.
  for(unsigned y=0;y<g.metrics.gmBlackBoxY;++y)for(unsigned x=0;x<g.metrics.gmBlackBoxX;++x){
   if(static_cast<std::size_t>(y)*g.stride+x>=g.pixels.size())return 7;
   if(g.pixels.at(static_cast<std::size_t>(y)*g.stride+x)>64)return 8;
  }
  for(auto alpha:g.pixels){if(alpha>64)return 4;if(alpha>0&&alpha<64)antialiased=true;}
 }
 if(!antialiased)return 5;
 std::cout<<"PASS: installed Segoe UI glyph extraction, advances and anti-aliased coverage\n";
}
