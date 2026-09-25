#include "system_glyphs.hpp"
#include <iostream>
int main(){
 system_text::Font font;
 if(font.faceName!=L"Segoe UI"){std::wcerr<<L"Unexpected font: "<<font.faceName;return 1;}
 bool antialiased=false;
 for(unsigned char c:std::string("0123456789.BLOCKF7: GRID | GREEN SUPPORTED CELLSWHITE ROUTEX-YAV")){
  auto& g=font.get(c);if(!g.valid||g.metrics.gmCellIncX<=0)return 2;
  if(c!=' '&&g.pixels.empty())return 3;
  for(auto alpha:g.pixels){if(alpha>64)return 4;if(alpha>0&&alpha<64)antialiased=true;}
 }
 if(!antialiased)return 5;
 std::cout<<"PASS: installed Segoe UI glyph extraction, advances and anti-aliased coverage\n";
}
