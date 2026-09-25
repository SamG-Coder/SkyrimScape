#pragma once
#include "system_glyphs.hpp"
#include "vector_text.hpp"
namespace system_text {
inline Font& font(){static Font instance;return instance;}
inline double width(std::string_view text,double scale){double result=0;for(unsigned char c:text)result+=font().get(c).metrics.gmCellIncX*scale/4.;return result;}
inline void draw(RE::GFxValue& clip,std::string_view text,double scale,double color){
 for(unsigned char c:text)if(c!='\n'&&!font().get(c).valid){vector_text::draw(clip,text,scale,color);return;}
 clip.Invoke("clear");double pen=0,line=0,pixel=scale/4.;
 auto vertex=[&](const char* method,double x,double y){const std::array<RE::GFxValue,2> p{RE::GFxValue(x),RE::GFxValue(y)};clip.Invoke(method,p);};
 for(unsigned char c:text){if(c=='\n'){pen=0;line+=40*pixel;continue;}const auto& g=font().get(c);
  if(g.pixels.empty()){pen+=g.metrics.gmCellIncX*pixel;continue;}
  for(unsigned y=0;y<g.metrics.gmBlackBoxY;++y)for(unsigned x=0;x<g.metrics.gmBlackBoxX;){
   auto alpha=g.pixels[y*g.stride+x];if(!alpha){++x;continue;}auto first=x;while(x<g.metrics.gmBlackBoxX&&g.pixels[y*g.stride+x]==alpha)++x;
   const std::array<RE::GFxValue,2> fill{RE::GFxValue(color),RE::GFxValue(alpha*100./64.)};clip.Invoke("beginFill",fill);
   double a=pen+(g.metrics.gmptGlyphOrigin.x+static_cast<int>(first))*pixel,b=line+(32-g.metrics.gmptGlyphOrigin.y+y)*pixel,w=(x-first)*pixel;
   vertex("moveTo",a,b);vertex("lineTo",a+w,b);vertex("lineTo",a+w,b+pixel);vertex("lineTo",a,b+pixel);vertex("lineTo",a,b);clip.Invoke("endFill");
  }pen+=g.metrics.gmCellIncX*pixel;
 }
}
}
