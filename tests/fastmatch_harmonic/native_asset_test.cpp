#include "CxGeoSO2ReferenceAsset.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace cxgeom::so2;
void check(bool ok){if(!ok)throw std::runtime_error("asset check failed");}
void reject(const std::function<void()>& f,const char* why){
 try{f();}catch(const std::invalid_argument& e){if(std::string(e.what())!=why)throw;return;}
 throw std::runtime_error(std::string("expected rejection: ")+why);
}
int main(){
 check(ReferenceSha256("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
 check(ReferenceSha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
 check(ReferenceSha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")=="248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
 Contour c;c.topology_verified=true;c.closed=true;
 for(int i=0;i<64;++i)c.points.emplace_back(80*cos(i*6.283185307179586/64),45*sin(i*6.283185307179586/64));
 for(auto m:{Method::Dft,Method::Efd}){
  ReferenceAsset a{Build(c,Config{},m),"synthetic ellipse; topology explicitly declared"};
  auto bytes=EncodeReference(a),sha=ReferenceSha256(bytes);
  auto b=DecodeReference(bytes,sha,Config{},m);
  check(EncodeReference(b)==bytes);check(Distance(a.descriptor,b.descriptor)==0);
  check(Match(a.descriptor,b.descriptor).succeeded);
  auto kept=b;
  reject([&]{b=DecodeReference(bytes+"x",sha,Config{},m);},"ASSET_SHA_MISMATCH");
  check(EncodeReference(b)==EncodeReference(kept));
  reject([&]{DecodeReference(bytes,"",Config{},m);},"ASSET_TRUSTED_SHA_REQUIRED");
  auto changed=Config{};changed.maximum_pose_residual=.04;
  reject([&]{DecodeReference(bytes,sha,changed,m);},"INCOMPATIBLE_CONFIG");
  reject([&]{DecodeReference(bytes,sha,Config{},m==Method::Dft?Method::Efd:Method::Dft);},"INCOMPATIBLE_METHOD");
  auto trailing=bytes+"x";
  reject([&]{DecodeReference(trailing,ReferenceSha256(trailing),Config{},m);},"ASSET_TRAILING_DATA");
  auto version=bytes;version.replace(version.find(".v1"),3,".v9");
  reject([&]{DecodeReference(version,ReferenceSha256(version),Config{},m);},"UNSUPPORTED_ASSET_SCHEMA");
  auto cut=bytes.substr(0,bytes.size()/2);
  reject([&]{DecodeReference(cut,ReferenceSha256(cut),Config{},m);},"MALFORMED_ASSET");
 }
 reject([&]{DecodeReference(std::string(262145,'x'),"",Config{},Method::Dft);},"ASSET_SIZE_LIMIT");
 std::cout<<"native reference asset guards PASS\n";
}
