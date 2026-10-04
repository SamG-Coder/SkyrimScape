#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <string>
#include <cstring>
namespace scape::anime {
using Microsoft::WRL::ComPtr;
inline constexpr char shaderSource[]=R"hlsl(
Texture2D<float4> scene : register(t0);
RWTexture2D<float4> result : register(u0);
float3 at(int2 p,int2 size){return scene.Load(int3(clamp(p,0,size-1),0)).rgb;}
float lum(float3 c){return dot(c,float3(.2126,.7152,.0722));}
[numthreads(8,8,1)] void main(uint3 id:SV_DispatchThreadID){
 uint w,h;result.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;
 int2 p=id.xy,size=int2(w,h);float4 original=scene.Load(int3(p,0));
 float3 c=original.rgb;
 // Smooth small texture detail without erasing silhouettes.
 float3 n=at(p+int2(0,-1),size),s=at(p+int2(0,1),size);
 float3 e=at(p+int2(1,0),size),v=at(p+int2(-1,0),size);
 float3 average=(c*4+n+s+e+v)/8;
 float y=lum(average);
 // Soft transitions between five cel bands retain detail in dark interiors.
 float band=floor(saturate(y)*5)/5;
 float fraction=frac(saturate(y)*5);
 float shade=band+smoothstep(.40,.60,fraction)/5;
 float3 toon=average*(lerp(y,shade,.65)+.012)/(y+.012);
 toon=lerp(lum(toon).xxx,toon,1.18);
 float edge=max(length(e-v),length(n-s));
 float ink=smoothstep(.16,.48,edge)*.70;
 result[p]=float4(saturate(lerp(toon,float3(.035,.045,.065),ink)),original.a);
}
)hlsl";
class Pass {
 ComPtr<ID3D11ComputeShader> shader;
 ComPtr<ID3D11Texture2D> input,output;
 ComPtr<ID3D11ShaderResourceView> srv;
 ComPtr<ID3D11UnorderedAccessView> uav;
 UINT width{},height{};DXGI_FORMAT format{};
 public:
 std::string error;
 HRESULT initialize(ID3D11Device* device){
  if(shader)return S_OK;
  ComPtr<ID3DBlob> code,errors;
  auto hr=D3DCompile(shaderSource,sizeof(shaderSource)-1,"SkyrimScapeAnime",nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
  if(FAILED(hr)){if(errors)error.assign(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize());return hr;}
  return device->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&shader);
 }
 HRESULT apply(ID3D11DeviceContext* context,ID3D11Texture2D* target){
  if(!context||!target||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return E_INVALIDARG;
  D3D11_TEXTURE2D_DESC desc{};target->GetDesc(&desc);
  if(desc.SampleDesc.Count!=1||desc.ArraySize!=1||desc.MipLevels!=1||!desc.Width||!desc.Height)return E_INVALIDARG;
  auto fmt=desc.Format;
  if(fmt==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||fmt==DXGI_FORMAT_R8G8B8A8_TYPELESS)fmt=DXGI_FORMAT_R8G8B8A8_UNORM;
  if(fmt!=DXGI_FORMAT_R8G8B8A8_UNORM&&fmt!=DXGI_FORMAT_R10G10B10A2_UNORM&&fmt!=DXGI_FORMAT_R16G16B16A16_FLOAT)return DXGI_ERROR_UNSUPPORTED;
  ComPtr<ID3D11Device> device;context->GetDevice(&device);auto hr=initialize(device.Get());if(FAILED(hr))return hr;
  if(width!=desc.Width||height!=desc.Height||format!=fmt){
   input.Reset();output.Reset();srv.Reset();uav.Reset();width=height=0;
   D3D11_TEXTURE2D_DESC td=desc;td.Format=fmt;td.Usage=D3D11_USAGE_DEFAULT;td.CPUAccessFlags=td.MiscFlags=0;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
   if(FAILED(hr=device->CreateTexture2D(&td,nullptr,&input)))return hr;
   td.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
   if(FAILED(hr=device->CreateTexture2D(&td,nullptr,&output)))return hr;
   if(FAILED(hr=device->CreateShaderResourceView(input.Get(),nullptr,&srv)))return hr;
   if(FAILED(hr=device->CreateUnorderedAccessView(output.Get(),nullptr,&uav)))return hr;
   width=desc.Width;height=desc.Height;format=fmt;
  }
  // Keep engine D3D state intact: no graphics shaders, textures, vertex/index
  // buffers, viewport, blend, rasterizer or depth state are replaced.
  std::array<ID3D11RenderTargetView*,8> rt{};ComPtr<ID3D11DepthStencilView> depth;
  context->OMGetRenderTargets(8,rt.data(),&depth);
  std::array<ID3D11UnorderedAccessView*,8> omUAV{};
  context->OMGetRenderTargetsAndUnorderedAccessViews(0,nullptr,nullptr,0,8,omUAV.data());
  UINT rtCount=0;for(UINT i=0;i<8;++i)if(rt[i])rtCount=i+1;
  ComPtr<ID3D11ComputeShader> oldShader;std::array<ID3D11ClassInstance*,256> classes{};UINT count=256;
  context->CSGetShader(&oldShader,classes.data(),&count);
  ComPtr<ID3D11ShaderResourceView> oldSRV;ComPtr<ID3D11UnorderedAccessView> oldUAV;
  context->CSGetShaderResources(0,1,&oldSRV);context->CSGetUnorderedAccessViews(0,1,&oldUAV);
  ComPtr<ID3D11Predicate> predicate;BOOL predicateValue{};context->GetPredication(&predicate,&predicateValue);
  context->SetPredication(nullptr,FALSE);context->OMSetRenderTargets(0,nullptr,nullptr);
  context->CopyResource(input.Get(),target);
  auto read=srv.Get();auto write=uav.Get();context->CSSetShader(shader.Get(),nullptr,0);
  context->CSSetShaderResources(0,1,&read);context->CSSetUnorderedAccessViews(0,1,&write,nullptr);
  context->Dispatch((width+7)/8,(height+7)/8,1);
  ID3D11ShaderResourceView* emptySRV{};ID3D11UnorderedAccessView* emptyUAV{};
  context->CSSetShaderResources(0,1,&emptySRV);context->CSSetUnorderedAccessViews(0,1,&emptyUAV,nullptr);
  context->CopyResource(target,output.Get());
  read=oldSRV.Get();write=oldUAV.Get();context->CSSetShaderResources(0,1,&read);context->CSSetUnorderedAccessViews(0,1,&write,nullptr);
  context->CSSetShader(oldShader.Get(),classes.data(),count);
  context->OMSetRenderTargetsAndUnorderedAccessViews(rtCount,rt.data(),depth.Get(),rtCount,8-rtCount,omUAV.data()+rtCount,nullptr);
  context->SetPredication(predicate.Get(),predicateValue);
  for(auto r:rt)if(r)r->Release();for(auto u:omUAV)if(u)u->Release();for(UINT i=0;i<count;++i)if(classes[i])classes[i]->Release();
  return S_OK;
 }
};
}
