#include "style_gpu.hpp"
#include "style_mesh.hpp"
#include <iostream>
#include <set>
#include <map>
#include <limits>
using namespace runtime_style;
int main(){
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
 if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)))return 1;
 D3D11_TEXTURE2D_DESC td{};td.Width=1024;td.Height=512;td.MipLevels=11;td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;td.SampleDesc.Count=1;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 ComPtr<ID3D11Texture2D> texture;ComPtr<ID3D11ShaderResourceView> source;
 if(FAILED(device->CreateTexture2D(&td,nullptr,&texture))||FAILED(device->CreateShaderResourceView(texture.Get(),nullptr,&source)))return 2;
 auto reduced=smallTexture(device.Get(),source.Get());if(!reduced)return 3;
 D3D11_SHADER_RESOURCE_VIEW_DESC rd{},sd{};reduced->GetDesc(&rd);source->GetDesc(&sd);
 if(rd.Format!=sd.Format||rd.Texture2D.MostDetailedMip!=4||rd.Texture2D.MipLevels!=7||sd.Texture2D.MostDetailedMip!=0)return 4;
 td.BindFlags|=D3D11_BIND_RENDER_TARGET;texture.Reset();source.Reset();
 if(FAILED(device->CreateTexture2D(&td,nullptr,&texture))||FAILED(device->CreateShaderResourceView(texture.Get(),nullptr,&source)))return 5;
 if(smallTexture(device.Get(),source.Get()))return 6;
 D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_ANISOTROPIC;sampler.MaxAnisotropy=16;
 sampler.AddressU=D3D11_TEXTURE_ADDRESS_WRAP;sampler.AddressV=D3D11_TEXTURE_ADDRESS_CLAMP;sampler.AddressW=D3D11_TEXTURE_ADDRESS_MIRROR;
 sampler.ComparisonFunc=D3D11_COMPARISON_ALWAYS;sampler.MaxLOD=D3D11_FLOAT32_MAX;
 ComPtr<ID3D11SamplerState> original;device->CreateSamplerState(&sampler,&original);
 auto pixels=pixelSampler(device.Get(),original.Get());if(!pixels)return 7;
 D3D11_SAMPLER_DESC pd{};pixels->GetDesc(&pd);
 if(pd.Filter!=D3D11_FILTER_MIN_MAG_MIP_POINT||pd.AddressU!=sampler.AddressU||pd.AddressV!=sampler.AddressV||pd.AddressW!=sampler.AddressW)return 8;
 sampler.Filter=D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;original.Reset();device->CreateSamplerState(&sampler,&original);
 if(pixelSampler(device.Get(),original.Get()))return 9;
 std::vector<float> positions;std::vector<std::uint16_t> input;
 for(unsigned y=0;y<17;++y)for(unsigned x=0;x<17;++x){positions.push_back(float(x));positions.push_back(float(y));positions.push_back(0);}
 for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x){std::uint16_t a=y*17+x,b=a+1,c=a+17,d=c+1;input.insert(input.end(),{a,b,c,b,d,c});}
 std::vector<unsigned char> vb(positions.size()*4),ib(input.size()*2);std::memcpy(vb.data(),positions.data(),vb.size());std::memcpy(ib.data(),input.data(),ib.size());
 auto output=simplify(vb,ib,12);if(output.empty()||output.size()>=input.size()||output.size()%3)return 10;
 auto boundary=[](const std::vector<std::uint16_t>& indices){
  std::map<std::pair<unsigned,unsigned>,unsigned> edges;std::set<std::pair<unsigned,unsigned>> result;
  for(std::size_t i=0;i<indices.size();i+=3)for(unsigned j=0;j<3;++j){unsigned a=indices[i+j],b=indices[i+(j+1)%3];if(a>b)std::swap(a,b);++edges[{a,b}];}
  for(auto [edge,count]:edges)if(count==1)result.insert(edge);return result;
 };
 if(boundary(input)!=boundary(output))return 11;
 for(auto index:output)if(index>=positions.size()/3)return 12;
 auto invalid=ib;invalid[0]=invalid[1]=255;if(!simplify(vb,invalid,12).empty())return 13;
 float nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(vb.data(),&nan,4);if(!simplify(vb,ib,12).empty())return 14;
 std::cout<<"PASS: WARP mip views preserve source/sRGB, render-target exclusion, point samplers preserve addressing/comparison; mesh reduction preserves perimeter and valid indices\n";
}
