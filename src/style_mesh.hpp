#pragma once
#include <meshoptimizer.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>
namespace runtime_style {
// Only float-position, immutable triangle lists enter here. Preserve boundaries
// and existing vertices (including UV seams); never decimate by dropping faces.
inline std::vector<std::uint16_t> simplify(const std::vector<unsigned char>& vertices,
 const std::vector<unsigned char>& indices,unsigned stride){
 if(stride<12||stride>128||vertices.empty()||vertices.size()%stride||indices.size()%6)return {};
 auto count=vertices.size()/stride;
 if(count>65535)return {};
 std::vector<float> positions(count*3);
 for(std::size_t i=0;i<count;++i){
  std::memcpy(positions.data()+i*3,vertices.data()+i*stride,12);
  for(int j=0;j<3;++j)if(!std::isfinite(positions[i*3+j]))return {};
 }
 std::vector<unsigned> input(indices.size()/2),output(input.size());
 for(std::size_t i=0;i<input.size();++i){std::uint16_t index;std::memcpy(&index,indices.data()+i*2,2);if(index>=count)return {};input[i]=index;}
 if(input.size()<96)return {};
 auto target=static_cast<std::size_t>(input.size()*.35)/3*3;
 auto size=meshopt_simplify(output.data(),input.data(),input.size(),positions.data(),count,12,target,.015f,meshopt_SimplifyLockBorder,nullptr);
 if(size<3||size>=input.size())return {};
 return {output.begin(),output.begin()+size};
}
}
