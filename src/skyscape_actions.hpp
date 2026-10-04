#pragma once
#include <cstdint>
namespace scape::skyscape {
enum class Resource { none,tree,ore };
// SkyScape 1.2.0 ACTI records verified against attached VMAD scripts.
// These are plugin-local IDs, never hard-coded load-order indices.
constexpr Resource resource(std::uint32_t id){
 switch(id){
 case 0x12ca:case 0x6db8:case 0x6db9:case 0x3be9a:case 0x3be9c:case 0x3be9e:case 0x3bea0:case 0x421e6:
 case 0x106d27:case 0x1eeede:case 0x1eeedf:case 0x1f19cd:case 0x212e61:case 0x2157f1:case 0x33b6bc:return Resource::tree;
 case 0xbd86:case 0xbd71:case 0x199bf:case 0x199c7:case 0x199c8:case 0x199c9:case 0x199ca:case 0x199cb:
 case 0x199cc:case 0x19f3c:case 0x30dd9b:case 0x1e9acf:return Resource::ore;
 default:return Resource::none;
 }
}
constexpr const char* label(Resource kind){return kind==Resource::tree?"Chop":"Mine";}
constexpr std::uint32_t tools(Resource kind){return kind==Resource::tree?0x29c5d5:kind==Resource::ore?0x2a40ff:0;}
constexpr const char* activationLabel(std::uint32_t id){
 switch(id){
 case 0x265fb7:case 0x265f9a:case 0x4bca:case 0x1acca0:case 0x1af8be:case 0x1af8ca:return "Fish";
 case 0x11a60:return "Pray";
 case 0x1c191:return "Smelt";
 case 0x2f4b4b:case 0x302839:case 0x313e97:case 0x313e9a:case 0x319282:case 0x31bc72:return "Craft runes";
 case 0x37e32:case 0x45e13:case 0x1e1a24:case 0x1e9b3d:case 0x1f1995:return "Enter altar";
 case 0x2f4659:case 0x30ad92:case 0x3310ef:return "Exit portal";
 default:return "Activate";
 }
}
}
