#pragma once
#include "CxSevenClassSupervision.h"

// Shared by the application registration and the real mu::Parser headless test.
template<class Parser> void RegisterSevenClassSupervision(Parser& parser) {
    CxSevenClassSupervision* host = nullptr;
    parser.DefineClass("SevenClassSupervision", host);
    parser.DefineClassFun("SevenClassSupervision", host, "loadrules", &CxSevenClassSupervision::loadrules);
    parser.DefineClassFun("SevenClassSupervision", host, "load", &CxSevenClassSupervision::load);
    parser.DefineClassFun("SevenClassSupervision", host, "run", &CxSevenClassSupervision::run);
    parser.DefineClassFun("SevenClassSupervision", host, "freeze", &CxSevenClassSupervision::freeze);
    parser.DefineClassFun("SevenClassSupervision", host, "savefreeze", &CxSevenClassSupervision::savefreeze);
    parser.DefineClassFun("SevenClassSupervision", host, "expectstatus", &CxSevenClassSupervision::expectstatus);
    parser.DefineClassFun("SevenClassSupervision", host, "save", &CxSevenClassSupervision::save);
    parser.DefineClassFun("SevenClassSupervision", host, "clear", &CxSevenClassSupervision::clear);
}
