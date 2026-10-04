#include "anime_pass.hpp"
#include <vector>
#include <iostream>
#include <fstream>
using namespace scape::anime;
int main(){
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
 if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)))return 1;
 const char* noop="[numthreads(1,1,1)] void main(uint3 id:SV_DispatchThreadID) {}";
 ComPtr<ID3DBlob> oldCode;ComPtr<ID3D11ComputeShader> oldShader;
 if(FAILED(D3DCompile(noop,std::strlen(noop),nullptr,nullptr,nullptr,"main","cs_5_0",0,0,&oldCode,nullptr))||FAILED(device->CreateComputeShader(oldCode->GetBufferPointer(),oldCode->GetBufferSize(),nullptr,&oldShader)))return 12;
 context->CSSetShader(oldShader.Get(),nullptr,0);
 Pass pass;auto hr=pass.initialize(device.Get());if(FAILED(hr)){std::cerr<<pass.error;return 2;}
 for(auto width:{64u,97u}){
  constexpr UINT height=32;std::vector<unsigned> pixels(width*height);
  for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x){unsigned r=x<width/2?180:40,g=x<width/2?95:150,b=x<width/2?60:190;pixels[y*width+x]=(127u<<24)|(b<<16)|(g<<8)|r;}
  D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET;
  D3D11_SUBRESOURCE_DATA data{pixels.data(),width*4,0};ComPtr<ID3D11Texture2D> target;ComPtr<ID3D11RenderTargetView> view;
  if(FAILED(device->CreateTexture2D(&d,&data,&target))||FAILED(device->CreateRenderTargetView(target.Get(),nullptr,&view)))return 3;
  auto rt=view.Get();context->OMSetRenderTargets(1,&rt,nullptr);
  if(FAILED(pass.apply(context.Get(),target.Get())))return 4;
  ComPtr<ID3D11RenderTargetView> restored;context->OMGetRenderTargets(1,&restored,nullptr);if(restored.Get()!=view.Get())return 5;
  ComPtr<ID3D11ComputeShader> cs;context->CSGetShader(&cs,nullptr,nullptr);if(cs.Get()!=oldShader.Get())return 6;
  d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.BindFlags=0;ComPtr<ID3D11Texture2D> staging;
  if(FAILED(device->CreateTexture2D(&d,nullptr,&staging)))return 7;context->CopyResource(staging.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
  if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))return 8;
  unsigned changed=0;float edge=0,interior=0;
  std::ofstream preview("anime-test-"+std::to_string(width)+".ppm",std::ios::binary);preview<<"P6\n"<<width<<" "<<height<<"\n255\n";
  for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x){auto value=reinterpret_cast<unsigned*>(static_cast<char*>(mapped.pData)+mapped.RowPitch*y)[x];
   if((value>>24)!=127)return 9;if(value!=pixels[y*width+x])++changed;
   unsigned char rgb[]{static_cast<unsigned char>(value),static_cast<unsigned char>(value>>8),static_cast<unsigned char>(value>>16)};preview.write(reinterpret_cast<char*>(rgb),3);
   if(y==16){float brightness=rgb[0]+rgb[1]+rgb[2];if(x==width/2)edge=brightness;if(x==width/2+8)interior=brightness;}
  }
  context->Unmap(staging.Get(),0);
  if(changed<width*height/2||edge>=interior)return 10;
 }
 if(pass.apply(context.Get(),nullptr)!=E_INVALIDARG)return 11;
 std::cout<<"PASS: anime shader compilation, GPU shading, edge ink, alpha preservation, resize and render-state restoration\n";
}
