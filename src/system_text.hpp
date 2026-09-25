#pragma once
#include "system_glyphs.hpp"
#include "vector_text.hpp"
namespace system_text {
inline Font& font(){static Font instance;return instance;}
inline double width(std::string_view text,double scale){double result=0;for(unsigned char c:text)result+=font().get(c).metrics.gmCellIncX*scale/4.;return result;}
inline void draw(RE::GFxValue& clip,std::string_view text,double scale,double color,bool outlined=false){
 for(unsigned char c:text)if(c!='\n'&&!font().get(c).valid){vector_text::draw(clip,text,scale,color);return;}
 clip.Invoke("clear");double pen=0,line=0,pixel=scale/4.;
 auto vertex=[&](const char* method,double x,double y){const std::array<RE::GFxValue,2> p{RE::GFxValue(x),RE::GFxValue(y)};clip.Invoke(method,p);};
 for(int pass=outlined?0:1;pass<2;++pass){pen=0;line=0;
 for(unsigned char c:text){if(c=='\n'){pen=0;line+=40*pixel;continue;}const auto& g=font().get(c);
  if(g.pixels.empty()){pen+=g.metrics.gmCellIncX*pixel;continue;}
  auto width=g.metrics.gmBlackBoxX+(pass==0?4:0),height=g.metrics.gmBlackBoxY+(pass==0?4:0),stride=pass==0?width:g.stride;
  const auto& pixels=pass==0?g.outline:g.pixels;int padding=pass==0?2:0;
  for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;){
   auto alpha=pixels[y*stride+x];if(!alpha){++x;continue;}auto first=x;while(x<width&&pixels[y*stride+x]==alpha)++x;
   const std::array<RE::GFxValue,2> fill{RE::GFxValue(pass==0?0.:color),RE::GFxValue(alpha*100./64.)};clip.Invoke("beginFill",fill);
   double a=pen+(g.metrics.gmptGlyphOrigin.x+static_cast<int>(first)-padding)*pixel,b=line+(32-g.metrics.gmptGlyphOrigin.y+static_cast<int>(y)-padding)*pixel,w=(x-first)*pixel;
   vertex("moveTo",a,b);vertex("lineTo",a+w,b);vertex("lineTo",a+w,b+pixel);vertex("lineTo",a,b+pixel);vertex("lineTo",a,b);clip.Invoke("endFill");
  }pen+=g.metrics.gmCellIncX*pixel;
 }
 }
}
}
