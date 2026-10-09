#include "CxGeoSO2ReferenceAsset.h"
#include <array>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
namespace cxgeom { namespace so2 {
namespace {
void need(bool v,const char* why) { if(!v)throw std::invalid_argument(why); }
constexpr size_t limit=262144;
uint32_t rotr(uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
void source(const std::string& s) {
    need(!s.empty() && s.size()<=1024,"INVALID_ASSET_PROVENANCE");
    for(unsigned char c:s)need(c>=32 && c<=126,"INVALID_ASSET_PROVENANCE");
}
}
std::string ReferenceSha256(const std::string& bytes) {
    need(bytes.size()<=limit,"ASSET_SIZE_LIMIT");
    return Sha256Bytes(bytes);
}
std::string Sha256Bytes(const std::string& bytes) {
    static constexpr uint32_t k[64]={
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    std::vector<unsigned char> v(bytes.begin(),bytes.end());
    const uint64_t bits=uint64_t(v.size())*8;
    v.push_back(0x80);while(v.size()%64!=56)v.push_back(0);
    for(int n=7;n>=0;--n)v.push_back(static_cast<unsigned char>(bits>>(8*n)));
    std::array<uint32_t,8> h={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    for(size_t off=0;off<v.size();off+=64) {
        uint32_t w[64];
        for(int i=0;i<16;++i)w[i]=(uint32_t(v[off+4*i])<<24)|(uint32_t(v[off+4*i+1])<<16)|(uint32_t(v[off+4*i+2])<<8)|v[off+4*i+3];
        for(int i=16;i<64;++i) {
            uint32_t a=w[i-15],b=w[i-2];
            w[i]=w[i-16]+(rotr(a,7)^rotr(a,18)^(a>>3))+w[i-7]+(rotr(b,17)^rotr(b,19)^(b>>10));
        }
        auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],z=h[7];
        for(int i=0;i<64;++i) {
            uint32_t t=z+(rotr(e,6)^rotr(e,11)^rotr(e,25))+((e&f)^((~e)&g))+k[i]+w[i];
            uint32_t u=(rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&b)^(a&c)^(b&c));
            z=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=z;
    }
    std::ostringstream s;s.imbue(std::locale::classic());s<<std::hex<<std::setfill('0');
    for(auto x:h)s<<std::setw(8)<<x;
    return s.str();
}
std::string EncodeReference(const ReferenceAsset& a) {
    Distance(a.descriptor,a.descriptor);source(a.provenance);
    const auto& d=a.descriptor;const auto& c=d.config;
    std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(17);
    s<<"cxvision.so2_reference.v1 AUDIT_ONLY\n"<<std::quoted(a.provenance)<<'\n'
     <<int(d.method)<<' '<<c.sample_count<<' '<<c.max_order<<' '<<c.minimum_source_points<<' '
     <<c.minimum_perimeter<<' '<<int(c.normalize_scale)<<' '<<c.maximum_pose_residual<<' '
     <<c.symmetry_relative_amplitude<<' '<<c.peak_relative_tolerance<<' '<<c.maximum_hypotheses<<'\n'
     <<d.centroid.real()<<' '<<d.centroid.imag()<<' '<<d.scale<<' '<<d.perimeter<<'\n'
     <<d.coefficients.size()<<'\n';
    for(size_t i=0;i<d.coefficients.size();++i)
        s<<d.frequencies[i]<<' '<<d.coefficients[i].real()<<' '<<d.coefficients[i].imag()<<'\n';
    need(s.str().size()<=limit,"ASSET_SIZE_LIMIT");return s.str();
}
ReferenceAsset DecodeReference(const std::string& bytes,const std::string& sha,
                               const Config& expected,Method method) {
    need(bytes.size()<=limit,"ASSET_SIZE_LIMIT");
    need(sha.size()==64,"ASSET_TRUSTED_SHA_REQUIRED");
    for(char c:sha)need((c>='0'&&c<='9')||(c>='a'&&c<='f'),"ASSET_TRUSTED_SHA_REQUIRED");
    need(ReferenceSha256(bytes)==sha,"ASSET_SHA_MISMATCH");
    std::istringstream s(bytes);s.imbue(std::locale::classic());
    std::string schema,mode; s>>schema>>mode;
    need(schema=="cxvision.so2_reference.v1" && mode=="AUDIT_ONLY","UNSUPPORTED_ASSET_SCHEMA");
    ReferenceAsset a;auto& d=a.descriptor;auto& c=d.config;
    s>>std::quoted(a.provenance);source(a.provenance);
    int m=-1,normal=-1;double x=0,y=0;size_t n=0;
    s>>m>>c.sample_count>>c.max_order>>c.minimum_source_points>>c.minimum_perimeter
     >>normal>>c.maximum_pose_residual>>c.symmetry_relative_amplitude>>c.peak_relative_tolerance>>c.maximum_hypotheses
     >>x>>y>>d.scale>>d.perimeter>>n;
    need(bool(s),"MALFORMED_ASSET");
    need(m==0||m==1,"UNSUPPORTED_METHOD");need(normal==0||normal==1,"MALFORMED_ASSET");
    need(c.max_order>=1 && c.max_order<=511 && n==size_t(2*c.max_order),"INVALID_DESCRIPTOR_SIZE");
    d.method=static_cast<Method>(m);c.normalize_scale=normal!=0;d.centroid={x,y};
    for(size_t i=0;i<n;++i){int k=0;double re=0,im=0;s>>k>>re>>im;need(bool(s),"MALFORMED_ASSET");d.frequencies.push_back(k);d.coefficients.emplace_back(re,im);}
    s>>std::ws;need(s.eof(),"ASSET_TRAILING_DATA");
    Distance(d,d);
    auto compatible=d;compatible.config=expected;compatible.method=method;
    Distance(d,compatible);
    // Require canonical encoding: rejects ignored fields, ambiguous numbers and locale drift.
    need(EncodeReference(a)==bytes,"NONCANONICAL_ASSET");
    return a; // No live model state is touched on failure.
}
}}
