#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <wrl/client.h>
#include <algorithm>
namespace runtime_style {
using Microsoft::WRL::ComPtr;
// Restrict a view to already resident small mips: no decode, file writes or
// full texture copies. Keep format/sRGB and alpha exactly as supplied by Skyrim.
inline ComPtr<ID3D11ShaderResourceView> smallTexture(ID3D11Device* device,ID3D11ShaderResourceView* source){
 ComPtr<ID3D11ShaderResourceView> result;
 if(!source)return result;
 D3D11_SHADER_RESOURCE_VIEW_DESC view{};source->GetDesc(&view);
 if(view.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D)return result;
 ComPtr<ID3D11Resource> resource;source->GetResource(&resource);
 ComPtr<ID3D11Texture2D> texture;if(FAILED(resource.As(&texture)))return result;
 D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
 if(desc.SampleDesc.Count!=1||desc.ArraySize!=1||desc.MipLevels<2||
    desc.BindFlags&(D3D11_BIND_RENDER_TARGET|D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_UNORDERED_ACCESS))return result;
 unsigned first=view.Texture2D.MostDetailedMip;
 unsigned levels=view.Texture2D.MipLevels==UINT(-1)?desc.MipLevels-first:view.Texture2D.MipLevels;
 unsigned last=(std::min)(desc.MipLevels,first+levels);
 while(first+1<last&&(std::max)(desc.Width>>first,desc.Height>>first)>64)++first;
 if(first==view.Texture2D.MostDetailedMip)return result;
 view.Texture2D.MostDetailedMip=first;view.Texture2D.MipLevels=last-first;
 device->CreateShaderResourceView(resource.Get(),&view,&result);return result;
}
inline ComPtr<ID3D11SamplerState> pixelSampler(ID3D11Device* device,ID3D11SamplerState* original){
 ComPtr<ID3D11SamplerState> result;if(!original)return result;
 D3D11_SAMPLER_DESC desc{};original->GetDesc(&desc);
 // Never convert comparison samplers (shadow maps).
 if((static_cast<unsigned>(desc.Filter)&0x180)!=0)return result;
 desc.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;desc.MaxAnisotropy=1;desc.MipLODBias=0;
 device->CreateSamplerState(&desc,&result);return result;
}
}
