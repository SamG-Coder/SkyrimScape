#pragma once
#include <array>
#include <string_view>
namespace vector_text {
// Original 5x7 bitmap glyphs drawn as vector rectangles: no font provider,
// shared SWF glyph library or operating-system font is required.
inline std::array<unsigned,7> glyph(char c){
 switch(c){
 case '0':return {14,17,19,21,25,17,14};case '1':return {4,12,4,4,4,4,14};case '2':return {14,17,1,2,4,8,31};case '3':return {30,1,1,14,1,1,30};case '4':return {2,6,10,18,31,2,2};case '5':return {31,16,16,30,1,1,30};case '6':return {14,16,16,30,17,17,14};case '7':return {31,1,2,4,8,8,8};case '8':return {14,17,17,14,17,17,14};case '9':return {14,17,17,15,1,1,14};
 case 'A':return {14,17,17,31,17,17,17};case 'B':return {30,17,17,30,17,17,30};case 'C':return {14,17,16,16,16,17,14};case 'D':return {30,17,17,17,17,17,30};case 'E':return {31,16,16,30,16,16,31};case 'F':return {31,16,16,30,16,16,16};case 'G':return {14,17,16,23,17,17,15};case 'H':return {17,17,17,31,17,17,17};case 'I':return {14,4,4,4,4,4,14};case 'J':return {7,2,2,2,2,18,12};case 'K':return {17,18,20,24,20,18,17};case 'L':return {16,16,16,16,16,16,31};case 'M':return {17,27,21,21,17,17,17};case 'N':return {17,25,21,19,17,17,17};case 'O':return {14,17,17,17,17,17,14};case 'P':return {30,17,17,30,16,16,16};case 'Q':return {14,17,17,17,21,18,13};case 'R':return {30,17,17,30,20,18,17};case 'S':return {15,16,16,14,1,1,30};case 'T':return {31,4,4,4,4,4,4};case 'U':return {17,17,17,17,17,17,14};case 'V':return {17,17,17,17,17,10,4};case 'W':return {17,17,17,21,21,21,10};case 'X':return {17,17,10,4,10,17,17};case 'Y':return {17,17,10,4,4,4,4};case 'Z':return {31,1,2,4,8,16,31};case '.':return {0,0,0,0,0,12,12};case ':':return {0,4,4,0,4,4,0};case '-':return {0,0,0,31,0,0,0};case '|':return {4,4,4,4,4,4,4};default:return {};
 }
}
inline void draw(RE::GFxValue& clip,std::string_view text,double scale,double color){
 clip.Invoke("clear");const std::array<RE::GFxValue,2> fill{RE::GFxValue(color),RE::GFxValue(100.)};clip.Invoke("beginFill",fill);
 auto vertex=[&](const char* method,double x,double y){const std::array<RE::GFxValue,2> p{RE::GFxValue(x),RE::GFxValue(y)};clip.Invoke(method,p);};
 double origin=0,line=0;
 for(char c:text){if(c=='\n'){origin=0;line+=9*scale;continue;}auto rows=glyph(c);
  for(int y=0;y<7;++y)for(int x=0;x<5;){if(!(rows[y]&(16u>>x))){++x;continue;}int first=x;while(x<5&&(rows[y]&(16u>>x)))++x;
   double a=origin+first*scale,b=line+y*scale,w=(x-first)*scale;
   vertex("moveTo",a,b);vertex("lineTo",a+w,b);vertex("lineTo",a+w,b+scale);vertex("lineTo",a,b+scale);vertex("lineTo",a,b);
  }origin+=6*scale;
 }clip.Invoke("endFill");
}
}
