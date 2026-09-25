#pragma once
#include <Windows.h>
#include <vector>
#include <string>
#include <unordered_map>
namespace system_text {
struct Glyph {GLYPHMETRICS metrics{};std::vector<unsigned char> pixels;unsigned stride{};bool valid{};};
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
   if(size!=GDI_ERROR){g.pixels.resize(size);g.stride=(g.metrics.gmBlackBoxX+3)&~3u;g.valid=size==0||GetGlyphOutlineW(dc,c,GGO_GRAY8_BITMAP,&g.metrics,size,g.pixels.data(),&matrix)!=GDI_ERROR;}
  }return cache.emplace(c,std::move(g)).first->second;
 }
};
}
