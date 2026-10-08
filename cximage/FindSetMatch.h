#pragma once
#include "../cxgeom/geometric_set_matching/types.h"
#include <string>
// Structured geometry adapter. No image extraction or production approval.
class FindSetMatch {
public:
    void load(const char* path);
    void requestjson(const char* json);
    void parameter(double value,const char* key);
    void run();
    void clear();
    void expectstatus(const char* status);
    void expectcount(int count);
    void save(const char* path);
    std::string receipt() const;
    const cxgeom::gsm::Result& result() const;
private:
    cxgeom::gsm::Request request_;
    cxgeom::gsm::Result result_;
    std::string snapshot_,input_sha_;
    bool loaded_=false,ran_=false;
    unsigned assertions_=0;
    void invalidate();
};
