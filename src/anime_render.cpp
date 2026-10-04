#include "pch.hpp"
#include "anime_render.hpp"
#include "anime_pass.hpp"
#include <atomic>
namespace anime_render {
namespace {
std::atomic_bool active{},enabled{true};
scape::anime::Pass pass;
using Fn=void(*)(std::int64_t);REL::Relocation<Fn> original;
void beforeInterface(std::int64_t arg){
 if(active.load()&&enabled.load()){
  auto renderer=RE::BSGraphics::Renderer::GetSingleton();
  if(renderer){
   auto& data=renderer->GetRuntimeData();
   auto swap=reinterpret_cast<IDXGISwapChain*>(data.renderWindows[0].swapChain);
   auto context=reinterpret_cast<ID3D11DeviceContext*>(data.context);
   scape::anime::ComPtr<ID3D11Texture2D> backbuffer;
   if(swap&&context&&SUCCEEDED(swap->GetBuffer(0,IID_PPV_ARGS(&backbuffer)))){
    auto hr=pass.apply(context,backbuffer.Get());
    static bool reported=false;
    if(!reported){reported=true;D3D11_TEXTURE2D_DESC d{};backbuffer->GetDesc(&d);
     spdlog::info("ANIME pre-interface pass result={:08X} size={}x{} format={} {}",static_cast<unsigned>(hr),d.Width,d.Height,static_cast<int>(d.Format),pass.error);
    }
    if(FAILED(hr)){enabled.store(false);spdlog::error("ANIME disabled after render failure {:08X}",static_cast<unsigned>(hr));}
   }
  }
 }
 original(arg);
}
}
void install(){
 // 1.7.104: main world-render caller immediately before DrawInterfaceStart.
 // Do not intercept loading/menu-only callers or swap-chain Present (after UI).
 constexpr std::uintptr_t rva=0x656921;
 constexpr std::array<unsigned char,5> expected{0xE8,0x5A,0x43,0xB1,0x00};
 auto site=REL::Module::get().base()+rva;
 if(std::memcmp(reinterpret_cast<void*>(site),expected.data(),expected.size())){spdlog::error("ANIME hook disabled: pre-interface call bytes differ");return;}
 if(SKSE::GetTrampoline().free_size()<32){spdlog::error("ANIME hook disabled: no reserved trampoline space");return;}
 auto renderer=RE::BSGraphics::Renderer::GetSingleton();
 auto device=renderer?reinterpret_cast<ID3D11Device*>(renderer->GetRuntimeData().forwarder):nullptr;
 if(!device||FAILED(pass.initialize(device))){spdlog::error("ANIME shader initialization failed: {}",pass.error);return;}
 original=SKSE::GetTrampoline().write_call<5>(site,beforeInterface);
 spdlog::info("ANIME pre-interface hook installed; F4 toggles in F8 mode");
}
void setGameplay(bool value){active.store(value);}
void toggle(){auto value=!enabled.load();enabled.store(value);spdlog::info("ANIME {}",value?"on":"off");}
}
