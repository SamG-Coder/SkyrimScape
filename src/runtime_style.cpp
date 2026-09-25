#include "pch.hpp"
#include "runtime_style.hpp"
#include "style_gpu.hpp"
#include "style_mesh.hpp"
#include <atomic>
#include <chrono>
#include <future>
#include <unordered_map>

namespace runtime_style {
namespace {
using Clock=std::chrono::steady_clock;
std::atomic_bool gameplay{},enabled{false};
struct Pass {
 std::array<ID3D11ShaderResourceView*,6> diffuse{};
 ID3D11ShaderResourceView* normal{};
 bool staticMesh{};
};
thread_local Pass pass;
using GeometryFn=void(*)(RE::BSShader*,RE::BSRenderPass*,std::uint32_t);
REL::Relocation<GeometryFn> originalSetup,originalRestore;
using DrawFn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,INT);
ID3D11DeviceContext* gameContext{};
ComPtr<ID3D11Device> device;
ComPtr<ID3D11ShaderResourceView> flatNormal;
template<class T> struct Cached {ComPtr<T> source,replacement;};
std::unordered_map<ID3D11ShaderResourceView*,Cached<ID3D11ShaderResourceView>> textures;
std::size_t textureBytes{};
std::unordered_map<ID3D11SamplerState*,Cached<ID3D11SamplerState>> samplers;
struct Mesh {
 ComPtr<ID3D11Buffer> vertices,indices,replacement;
 unsigned stride{},count{},originalCount{},bytes{};
};
std::unordered_map<ID3D11Buffer*,Mesh> meshes;
std::size_t meshBytes{};
constexpr std::size_t meshBudget=64*1024*1024;
struct Pending {
 Mesh mesh;
 ComPtr<ID3D11Buffer> vertices,indices;
 std::future<std::vector<std::uint16_t>> result;
};
std::unique_ptr<Pending> pending;
Clock::time_point nextWork{};
Clock::time_point nextPoll{};
unsigned reducedMeshes{},changedDraws{};
std::size_t retainedTextureBytes(const D3D11_TEXTURE2D_DESC& desc){
 // Block-compressed world textures dominate Skyrim's data. Include all mips.
 unsigned bits=128;
 switch(desc.Format){
 case DXGI_FORMAT_BC1_UNORM:case DXGI_FORMAT_BC1_UNORM_SRGB:case DXGI_FORMAT_BC4_UNORM:case DXGI_FORMAT_BC4_SNORM:bits=4;break;
 case DXGI_FORMAT_BC2_UNORM:case DXGI_FORMAT_BC2_UNORM_SRGB:case DXGI_FORMAT_BC3_UNORM:case DXGI_FORMAT_BC3_UNORM_SRGB:
 case DXGI_FORMAT_BC5_UNORM:case DXGI_FORMAT_BC5_SNORM:case DXGI_FORMAT_BC6H_UF16:case DXGI_FORMAT_BC6H_SF16:
 case DXGI_FORMAT_BC7_UNORM:case DXGI_FORMAT_BC7_UNORM_SRGB:bits=8;break;
 case DXGI_FORMAT_R8G8B8A8_UNORM:case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:case DXGI_FORMAT_B8G8R8A8_UNORM:case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:bits=32;break;
 }
 return static_cast<std::size_t>(desc.Width)*desc.Height*bits/8*2;
}

ID3D11ShaderResourceView* view(RE::NiSourceTexture* texture){
 return texture&&texture->rendererTexture?reinterpret_cast<ID3D11ShaderResourceView*>(texture->rendererTexture->resourceView):nullptr;
}
void setup(RE::BSShader* shader,RE::BSRenderPass* current,std::uint32_t flags){
 static std::atomic_bool reported{};
 if(!reported.exchange(true))spdlog::info("Runtime style diagnostic: lighting geometry hook reached");
 pass={};originalSetup(shader,current,flags);
 if(!enabled.load(std::memory_order_relaxed)||!gameplay.load(std::memory_order_relaxed)||!current||!current->shaderProperty)return;
 auto material=current->shaderProperty->material;
 if(!material||material->GetType()!=RE::BSShaderMaterial::Type::kLighting)return;
 auto base=static_cast<RE::BSLightingShaderMaterialBase*>(material);
 static std::atomic_bool materialReported{};
 if(!materialReported.exchange(true))spdlog::info("Runtime style diagnostic: active lighting material reached, feature={}",static_cast<int>(material->GetFeature()));
 pass.diffuse[0]=view(base->diffuseTexture.get());
 auto feature=material->GetFeature();
 if(feature==RE::BSShaderMaterial::Feature::kMultiTexLandLODBlend){
  auto land=static_cast<RE::BSLightingShaderMaterialLandscape*>(base);
  for(unsigned i=0;i<5;++i)pass.diffuse[i+1]=view(land->landscapeDiffuseTexture[i].get());
 }
 // Tangent-space default materials only. Model-space normal maps and special
 // shaders encode other data and must not receive a generic normal texture.
 if(feature==RE::BSShaderMaterial::Feature::kDefault&&
    !current->shaderProperty->flags.any(RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals))
  pass.normal=view(base->normalTexture.get());
 auto geometry=current->geometry;
 if(geometry&&geometry->GetType()==RE::BSGeometry::Type::kTriShape){
  auto& data=geometry->GetGeometryRuntimeData();
  pass.staticMesh=!data.skinInstance&&!data.alphaProperty&&
   !data.vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_SKINNED)&&
   data.vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_FULLPREC)&&
   feature==RE::BSShaderMaterial::Feature::kDefault;
 }
}
void restore(RE::BSShader* shader,RE::BSRenderPass* current,std::uint32_t flags){pass={};originalRestore(shader,current,flags);}

bool staging(ID3D11Buffer* source,ComPtr<ID3D11Buffer>& result){
 D3D11_BUFFER_DESC desc{};source->GetDesc(&desc);
 if(!desc.ByteWidth||desc.ByteWidth>4*1024*1024||desc.Usage==D3D11_USAGE_DYNAMIC||desc.CPUAccessFlags)return false;
 desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.MiscFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.StructureByteStride=0;
 return SUCCEEDED(device->CreateBuffer(&desc,nullptr,&result));
}
void poll(ID3D11DeviceContext* context){
 if(!pending||Clock::now()<nextPoll)return;
 nextPoll=Clock::now()+std::chrono::milliseconds(4);
 auto& job=*pending;
 if(job.result.valid()){
  if(job.result.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
  auto indices=job.result.get();
  if(!indices.empty()){
   D3D11_BUFFER_DESC desc{};desc.ByteWidth=static_cast<UINT>(indices.size()*2);desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_INDEX_BUFFER;
   D3D11_SUBRESOURCE_DATA data{indices.data(),0,0};
   if(SUCCEEDED(device->CreateBuffer(&desc,&data,&job.mesh.replacement))){job.mesh.count=static_cast<unsigned>(indices.size());job.mesh.bytes+=desc.ByteWidth;++reducedMeshes;}
  }
  bool reduced=bool(job.mesh.replacement);
  meshBytes+=job.mesh.bytes;
  meshes.emplace(job.mesh.indices.Get(),std::move(job.mesh));pending.reset();
  if(reduced&&(reducedMeshes==1||reducedMeshes%100==0))spdlog::info("Runtime style: {} simplified static meshes, {} MiB retained mesh buffers",reducedMeshes,meshBytes/(1024*1024));
  return;
 }
 D3D11_MAPPED_SUBRESOURCE v{},i{};
 auto vr=context->Map(job.vertices.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&v);
 if(vr==DXGI_ERROR_WAS_STILL_DRAWING)return;
 if(FAILED(vr)){pending.reset();return;}
 auto ir=context->Map(job.indices.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&i);
 if(FAILED(ir)){context->Unmap(job.vertices.Get(),0);if(ir!=DXGI_ERROR_WAS_STILL_DRAWING)pending.reset();return;}
 D3D11_BUFFER_DESC vd{},id{};job.vertices->GetDesc(&vd);job.indices->GetDesc(&id);
 std::vector<unsigned char> vertices(vd.ByteWidth),indices(id.ByteWidth);
 std::memcpy(vertices.data(),v.pData,vertices.size());std::memcpy(indices.data(),i.pData,indices.size());
 context->Unmap(job.vertices.Get(),0);context->Unmap(job.indices.Get(),0);
 job.vertices.Reset();job.indices.Reset();
 job.result=std::async(std::launch::async,[vertices=std::move(vertices),indices=std::move(indices),stride=job.mesh.stride]{return simplify(vertices,indices,stride);});
}
Mesh* mesh(ID3D11DeviceContext* context,ID3D11Buffer* indices,unsigned count){
 ComPtr<ID3D11Buffer> vertices;unsigned stride{},offset{};context->IAGetVertexBuffers(0,1,&vertices,&stride,&offset);
 if(!vertices||offset||stride<12||stride>128)return nullptr;
 if(auto found=meshes.find(indices);found!=meshes.end()){
  auto& entry=found->second;return entry.vertices.Get()==vertices.Get()&&entry.stride==stride&&entry.originalCount==count?&entry:nullptr;
 }
 if(pending||meshBytes>=meshBudget||meshes.size()>=2048||Clock::now()<nextWork)return nullptr;
 nextWork=Clock::now()+std::chrono::milliseconds(50);
 D3D11_BUFFER_DESC vd{},id{};vertices->GetDesc(&vd);indices->GetDesc(&id);
 if(id.ByteWidth!=count*2||vd.ByteWidth%stride||vd.ByteWidth/stride>65535||meshBytes+vd.ByteWidth+id.ByteWidth*2>meshBudget)return nullptr;
 auto job=std::make_unique<Pending>();
 if(!staging(vertices.Get(),job->vertices)||!staging(indices,job->indices))return nullptr;
 job->mesh.vertices=vertices;job->mesh.indices=indices;job->mesh.stride=stride;job->mesh.originalCount=count;job->mesh.bytes=vd.ByteWidth+id.ByteWidth;
 context->CopyResource(job->vertices.Get(),vertices.Get());context->CopyResource(job->indices.Get(),indices);
 pending=std::move(job);nextWork=Clock::now()+std::chrono::milliseconds(50);return nullptr;
}
template<class Submit>
void styledDraw(ID3D11DeviceContext* context,UINT count,UINT start,INT base,Submit submit){
 static std::atomic_bool reported{};
 if(!reported.exchange(true))spdlog::info("Runtime style diagnostic: indexed draw hook reached; selectedContext={}",context==gameContext);
 if(context!=gameContext||!enabled.load(std::memory_order_relaxed)||!gameplay.load(std::memory_order_relaxed)||!pass.diffuse[0]){submit(count);return;}
 // All changes are scoped to this draw. In particular, Skyrim's state cache
 // still sees the original bindings when we return, as do UI and shadow draws.
 poll(context);
 // Trim only between draws: replacement pointers below must stay alive until
 // bindings have been restored. This caps references retained across cell loads.
 if(textureBytes>240*1024*1024||textures.size()>=2047){textures.clear();textureBytes=0;}
 std::array<ID3D11ShaderResourceView*,16> original{},replacement{};
 context->PSGetShaderResources(0,16,original.data());replacement=original;
 bool changed=false;
 for(unsigned slot=0;slot<16;++slot){
  auto source=original[slot];if(!source)continue;
  if(source==pass.normal&&flatNormal){replacement[slot]=flatNormal.Get();changed=true;continue;}
  if(std::find(pass.diffuse.begin(),pass.diffuse.end(),source)==pass.diffuse.end())continue;
  auto found=textures.find(source);
  if(found==textures.end()&&textures.size()<2048){
   ComPtr<ID3D11Resource> resource;source->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;
   if(SUCCEEDED(resource.As(&texture))){
    D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
    // Conservative accounting for retained world color resources, including mip chains.
    auto bytes=retainedTextureBytes(desc);
    if(textureBytes+bytes<=256*1024*1024){
     Cached<ID3D11ShaderResourceView> entry;entry.source=source;entry.replacement=smallTexture(device.Get(),source);
     found=textures.emplace(source,std::move(entry)).first;textureBytes+=bytes;
    }
   }
  }
  if(found!=textures.end()&&found->second.replacement){replacement[slot]=found->second.replacement.Get();changed=true;}
 }
 std::array<ID3D11SamplerState*,16> oldSamplers{},newSamplers{};
 if(changed){
  context->PSSetShaderResources(0,16,replacement.data());
  context->PSGetSamplers(0,16,oldSamplers.data());newSamplers=oldSamplers;
  // Texture and sampler registers are independent. Preserve all addressing and
  // comparison state; point-filter ordinary sampling only within this world draw.
  for(unsigned slot=0;slot<16;++slot)if(auto source=oldSamplers[slot]){
   auto found=samplers.find(source);
   if(found==samplers.end()&&samplers.size()<128){Cached<ID3D11SamplerState> entry;entry.source=source;entry.replacement=pixelSampler(device.Get(),source);found=samplers.emplace(source,std::move(entry)).first;}
   if(found!=samplers.end()&&found->second.replacement)newSamplers[slot]=found->second.replacement.Get();
  }
  context->PSSetSamplers(0,16,newSamplers.data());
  if(++changedDraws==1)spdlog::info("Runtime style: first world draw styled (64px diffuse mips, point filtering)");
 }
 ComPtr<ID3D11Buffer> oldIndices;DXGI_FORMAT format{};UINT offset{};bool replacedMesh=false;
 if(pass.staticMesh&&!start&&!base&&count>=96){
  D3D11_PRIMITIVE_TOPOLOGY topology{};context->IAGetPrimitiveTopology(&topology);
  if(topology==D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST){
   context->IAGetIndexBuffer(&oldIndices,&format,&offset);
   if(oldIndices&&format==DXGI_FORMAT_R16_UINT&&!offset)if(auto reduced=mesh(context,oldIndices.Get(),count);reduced&&reduced->replacement){
    context->IASetIndexBuffer(reduced->replacement.Get(),format,0);count=reduced->count;replacedMesh=true;
   }
  }
 }
 submit(count);
 if(replacedMesh)context->IASetIndexBuffer(oldIndices.Get(),format,offset);
 if(changed){context->PSSetShaderResources(0,16,original.data());context->PSSetSamplers(0,16,oldSamplers.data());}
 for(auto source:original)if(source)source->Release();
 for(auto source:oldSamplers)if(source)source->Release();
}
void STDMETHODCALLTYPE draw(ID3D11DeviceContext* context,UINT count,UINT start,INT base){
 // Resolve the live COM entry at submission time so later overlay wrappers are
 // respected. We no longer change the driver's shared vtable.
 auto native=reinterpret_cast<DrawFn>((*reinterpret_cast<std::uintptr_t**>(context))[12]);
 styledDraw(context,count,start,base,[&](UINT amount){native(context,amount,start,base);});
}
void STDMETHODCALLTYPE drawInstanced(ID3D11DeviceContext* context,UINT count,UINT instances,UINT start,INT base,UINT firstInstance){
 static std::atomic_bool reported{};
 if(!reported.exchange(true))spdlog::info("Runtime style diagnostic: instanced triangle submission reached, instances={}",instances);
 using Fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT,INT,UINT);
 auto native=reinterpret_cast<Fn>((*reinterpret_cast<std::uintptr_t**>(context))[20]);
 styledDraw(context,count,start,base,[&](UINT amount){native(context,amount,instances,start,base,firstInstance);});
}
}
void install(){
 auto renderer=RE::BSGraphics::Renderer::GetSingleton();if(!renderer)return;
 // Verified 1.7.104 draw routines load this active-context global. It aliases
 // Renderer's context in the observed session; it was not the original bug.
 REL::Relocation<ID3D11DeviceContext**> activeContext{REL::ID(411391)};
 gameContext=*activeContext;
 if(!gameContext)return;
 spdlog::info("Runtime style context: active={} renderer={} type={}",static_cast<void*>(gameContext),static_cast<void*>(renderer->GetRuntimeData().context),static_cast<unsigned>(gameContext->GetType()));
 gameContext->GetDevice(&device);
 // Neutral tangent normal, zero specular alpha. No shader code or material
 // constants are patched, and model-space normal maps are excluded above.
 const std::uint32_t pixel=0x00FF8080;
 D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 D3D11_SUBRESOURCE_DATA data{&pixel,4,4};ComPtr<ID3D11Texture2D> texture;
 if(SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture)))device->CreateShaderResourceView(texture.Get(),nullptr,&flatNormal);
 // 1.7.104-only sites verified against SkyrimSE.exe. Cover both ordinary
 // indexed triangles and instanced triangle submission. Check ALL sites before
 // installing any: fail closed if another mod/runtime changed these bytes.
 struct Site{std::uintptr_t rva,hook;std::array<unsigned char,6> expected;};
 const std::array<Site,3> sites{{
  {0x100BDA7,reinterpret_cast<std::uintptr_t>(draw),{0x48,0x8B,0x01,0xFF,0x50,0x60}},
  {0x100BEAB,reinterpret_cast<std::uintptr_t>(draw),{0x48,0x8B,0x01,0xFF,0x50,0x60}},
  {0x100BFD6,reinterpret_cast<std::uintptr_t>(drawInstanced),{0xFF,0x90,0xA0,0,0,0}}
 }};
 auto module=REL::Module::get().base();
 for(auto& site:sites)if(std::memcmp(reinterpret_cast<void*>(module+site.rva),site.expected.data(),6)){
  spdlog::error("Runtime style disabled: triangle submission bytes differ at {:X}",site.rva);return;
 }
 SKSE::AllocTrampoline(64);
 std::array<std::array<unsigned char,6>,3> patches{};
 for(std::size_t i=0;i<sites.size();++i){
  auto slot=SKSE::GetTrampoline().allocate(sizeof(std::uintptr_t));std::memcpy(slot,&sites[i].hook,sizeof(std::uintptr_t));
  auto delta=reinterpret_cast<std::intptr_t>(slot)-static_cast<std::intptr_t>(module+sites[i].rva+6);
  if(delta<INT32_MIN||delta>INT32_MAX){spdlog::error("Runtime style disabled: trampoline out of reach");return;}
  patches[i][0]=0xFF;patches[i][1]=0x15;auto displacement=static_cast<std::int32_t>(delta);std::memcpy(patches[i].data()+2,&displacement,4);
 }
 for(std::size_t i=0;i<sites.size();++i){
  auto address=module+sites[i].rva;REL::safe_write(address,patches[i].data(),6);
  FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),6);
 }
 REL::Relocation<std::uintptr_t> lighting{RE::VTABLE_BSLightingShader[0]};
 originalSetup=lighting.write_vfunc(6,setup);originalRestore=lighting.write_vfunc(7,restore);
 spdlog::info("Runtime style hooks installed at 3 verified triangle submission sites; F6 toggles while F8 is active.");
}
void setGameplay(bool active){gameplay.store(active,std::memory_order_relaxed);}
void toggle(){bool value=!enabled.load();enabled.store(value);spdlog::info("Runtime Old School style {}",value?"on":"off");}
}
