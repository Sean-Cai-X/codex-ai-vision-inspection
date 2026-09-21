#pragma once

#include "CxExecutionTypes.h"
#include "CxTorchExecutionAdapter.h"
#include "Image.h"
#include <string>

class TorchTask
{
public:
    void settask(const char* value);
    void setcase(const char* value);
    void setmodel(const char* value);
    void setmanifest(const char* value);
    void setdatasetroot(const char* value);
    void setdevice(const char* value);
    void setinputpath(const char* value);
    void settemplatepath(const char* value);
    void setoutputdir(const char* value);
    void setrequestcontext(const char* value);
    void settimeout(int value);

    void run(void* image_object);

    int getok();
    int geterrorcode();
    int getresultcount();
    int getmaskavailable();

    double getinferms();
    double gettrainms();
    double gettotalms();

    char* getstatus();
    char* getreason();
    char* getfailstage();
    char* getactualdevice();
    char* getresultref();
    char* getevidenceref();
    char* getprimaryvisualref();
    char* getmaskref();
    char* getoverlayref();
    char* gettrainersummary();
    char* getmainlinesummary();

    const CxInferenceResult& GetInferenceResult() const noexcept;

private:
    CxTorchTaskSpec task_;
    CxInferenceResult result_;
    CxTorchExecutionAdapter executor_;

    std::string status_;
    std::string reason_;
};
