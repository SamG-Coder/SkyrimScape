#include "NifFile.hpp"
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <unordered_map>
namespace fs = std::filesystem;
int main(int argc, char** argv) {
 if(argc!=4) { std::cerr << "Usage: port_meshes input-meshes output-meshes texture-fixes.tsv\n"; return 2; }
 fs::create_directories(fs::path(argv[2]));
 int count=0, failures=0;
 std::unordered_map<std::string,std::string> fixes;
 std::ifstream fixFile(argv[3]);
 if(!fixFile) { std::cerr << "Cannot open texture fixes\n"; return 2; }
 std::string fixLine;
 while(std::getline(fixFile,fixLine)){auto tab=fixLine.find('\t');if(tab!=std::string::npos)fixes[fixLine.substr(0,tab)]=fixLine.substr(tab+1);}
 std::ofstream textures(fs::path(argv[2]).parent_path()/"mesh-textures.tsv");
 for(auto& e:fs::recursive_directory_iterator(argv[1])) {
  auto ext=e.path().extension().string(); std::transform(ext.begin(),ext.end(),ext.begin(),::tolower);
  if(!e.is_regular_file()||ext!=".nif") continue;
  auto rel=fs::relative(e.path(),argv[1]); auto dest=fs::path(argv[2])/rel;
  try {
   nifly::NifFile n;
   if(n.Load(e.path())!=0 || n.HasUnknown()) throw std::runtime_error("load/unknown blocks");
   auto shapes=n.GetShapes().size();
   nifly::OptOptions opt; opt.targetVersion=nifly::NiVersion::getSSE();
   auto name=rel.generic_string(); std::transform(name.begin(),name.end(),name.begin(),::tolower);
   opt.headParts=name.find("facegeom/")!=std::string::npos;
   if(n.OptimizeFor(opt).versionMismatch) throw std::runtime_error("unsupported version");
   for(auto* shape:n.GetShapes())for(auto& path:n.GetTexturePathRefs(shape)) {
    auto key=path.get();std::transform(key.begin(),key.end(),key.begin(),::tolower);
    if(auto it=fixes.find(key);it!=fixes.end())path.get()=it->second=="<clear>"?"":it->second;
   }
   fs::create_directories(dest.parent_path());
   if(n.Save(dest)!=0) throw std::runtime_error("save");
   nifly::NifFile check;
   if(check.Load(dest)!=0 || !check.GetHeader().GetVersion().IsSSE() || check.GetShapes().size()!=shapes)
    throw std::runtime_error("roundtrip validation");
   for(auto* shape:check.GetShapes())for(auto& path:check.GetTexturePathRefs(shape))
    if(!path.get().empty())textures<<rel.generic_string()<<'\t'<<path.get()<<'\n';
   ++count;
  } catch(const std::exception& ex) {++failures;std::cout<<"FAIL "<<rel<<" "<<ex.what()<<std::endl;}
 }
 std::cout<<"Converted and verified "<<count<<" meshes; failures "<<failures<<std::endl;
 return failures?1:0;
}
