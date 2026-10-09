#include "synthetic_materializer.h"
#include <iostream>
int main(int argc,char** argv) {
    try {
        if(argc!=3) throw std::runtime_error("usage: materializer rules.json output_base");
        std::ifstream f(argv[1]);std::string bytes((std::istreambuf_iterator<char>(f)),{});
        auto rules=cxvision::supervision::ParseJson(bytes);
        auto root=supervision_fixture::fresh_root(argv[2]);
        auto index=supervision_fixture::make_suite(root,rules);
        std::cout<<"Native synthetic materialization complete: "<<root.generic_string()<<'\n';
        return index.at("cases").size()==7?0:1;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
