#pragma once
#include <Windows.h>
#include <vector>
#include <string>
#include <unordered_map>
namespace system_text {
struct Glyph {GLYPHMETRICS metrics{};std::vector<unsigned char> pixels,outline;unsigned stride{};bool valid{};};
class Font {
 HDC dc{};HFONT font{};HGDIOBJ previous{};
 std::unordered_map<unsigned char,Glyph> cache;
public:
 std::wstring faceName;
 Font(){
  dc=CreateCompatibleDC(nullptr);if(!dc)return;
  font=CreateFontW(-32,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  if(!font)return;previous=SelectObject(dc,font);
  wchar_t name[128]{};GetTextFaceW(dc,128,name);faceName=name;
 }
 ~Font(){if(dc&&previous)SelectObject(dc,previous);if(font)DeleteObject(font);if(dc)DeleteDC(dc);}
 const Glyph& get(unsigned char c){
  if(auto found=cache.find(c);found!=cache.end())return found->second;
  Glyph g;MAT2 matrix{{0,1},{0,0},{0,0},{0,1}};
  if(dc&&font){auto size=GetGlyphOutlineW(dc,c,GGO_GRAY8_BITMAP,&g.metrics,0,nullptr,&matrix);
   if(size!=GDI_ERROR){
    g.pixels.resize(size);g.stride=(g.metrics.gmBlackBoxX+3)&~3u;
    // GDI reports a 1x1 black box for space but returns no bitmap. Preserve
    // its advance while ensuring consumers never iterate nonexistent pixels.
    if(size==0){g.metrics.gmBlackBoxX=0;g.metrics.gmBlackBoxY=0;g.stride=0;g.valid=true;}
    else{
     g.valid=GetGlyphOutlineW(dc,c,GGO_GRAY8_BITMAP,&g.metrics,size,g.pixels.data(),&matrix)!=GDI_ERROR;
     if(!g.stride||g.metrics.gmBlackBoxY>g.pixels.size()/g.stride||g.metrics.gmBlackBoxX>g.stride)g.valid=false;
    }
   }
  }
  if(g.valid&&!g.pixels.empty()){
   auto width=g.metrics.gmBlackBoxX+4,height=g.metrics.gmBlackBoxY+4;g.outline.assign(static_cast<std::size_t>(width)*height,0);
   // Dilate the anti-aliased coverage by two source pixels (1.5 HUD pixels at
   // combat size). Cache one outline mask instead of drawing eight text copies.
   for(unsigned y=0;y<g.metrics.gmBlackBoxY;++y)for(unsigned x=0;x<g.metrics.gmBlackBoxX;++x){
    auto alpha=g.pixels[y*g.stride+x];if(!alpha)continue;
    for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx){if(dx*dx+dy*dy>5)continue;
     auto& dst=g.outline[(y+2+dy)*width+x+2+dx];if(alpha>dst)dst=alpha;
    }
   }
  }
  return cache.emplace(c,std::move(g)).first->second;
 }
};
}
