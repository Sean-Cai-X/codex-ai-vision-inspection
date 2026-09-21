#ifndef PARSERCLASS_H
#define PARSERCLASS_H
#include "muParser.h"
#include <string>
#include <vector>

using namespace std;
#define WM_ZOOMWIN 100001

namespace mu
{
    class CxParserRuntime
    {
    public:
        CxParserRuntime();
        ~CxParserRuntime() {};

        CxParserRuntime(const CxParserRuntime&) = delete;
        CxParserRuntime& operator=(const CxParserRuntime&) = delete;

        double CONST_TAN(int ix, int iy);
        void ParserInitialClassFunction(int iusing);
        void ParserElementShow(int ishow);

    public:
        mu::Parser m_parser;

        value_type Eval();

        void SetExpr(const string & str);

        double GetResult();

        void DefineVar(const string & str,double *dvalue);

        void DefineStringConstant(
            const string& name,
            const string& value);

        static double* AddVariable(const char *a_szName,void *pClass);

        void SetVarFactory();

        ParserByteCode::storage_type GetByteCode();

        double RunByteCode(ParserByteCode::storage_type Base);

        void* GetClassObj(const string &strclass,const string & strobj);

        void* GetClassObj(const string &strclass,const int &iobjnum);

        void* GetDoubleValue(const string &strname);

        int GetClassObjSum(const string &strclass);

        std::string GetClassObjName(
            const std::string& class_name,
            int object_index) const;

        void* GetClass(const string &strclass);
    private:
        int g_testcal ;
        double m_afValBuf[100];
        int m_iVal;
        char m_line[100];
        int m_iget;
        void ListFunction(const mu::Parser  * pParser);
        void ListExprVar(const mu::Parser *parser);
        void ListConst(const mu::Parser *parser);
        void ListVar(const mu::Parser *parser);
        void ListClass(mu::Parser  &Pparser);
        void ListFormula(mu::Parser &parser);
        void FindClassObject(mu::Parser &Pparser,const char *a_szClass);
    public:
        void FunTest(void *func);
        void * m_Func;
        void SelfTest();
        void ListClass();
        void ListVar();
        void ListClassONLYName();
        void ListClassONLYFUNCTION();
        bool CommandLine(const string & szLine);
        void ShowHelp();
        void Calc(const char *pszFormula);
        bool Compile(const char *a_szLine);
        bool CompileCollectedScript(const std::string& script, std::string& reason);
        bool RunCollectedScript(std::string& reason);
        void ClearAll();
        void StopRun();
        void SetRunOk();
        //
        void SetStream(std::ostream *a_stream);
        void SetCreateCodeStream(std::ostream *a_stream);
#if defined(_WIN32x)
        void ShowImageParserElement(HDC Hdc,int itype=3);
        void ShowImageParserElementA(HDC Hdc,int ix=0,int iy=0,int cx=-1,int cy=-1,int itype=3);
        void ShowParserGDIimage(HDC Hdc);
#endif
        void DragImageParserElement(int ipointx,int ipointy);
        void HitTestImageParserElement(int ipointx,int ipointy);
        void MouseDownParserElement(int ipointx,int ipointy);
        void MouseUpParserElement(int PointX,int PointY);
        bool MouseRDownParserElement();

        void GetImageObjectAtt();
        //
        double RunOptCode();
        void RunFastCode();
        void SetOptCollect(bool Open_Close);
        void SetByteCollection(bool btruefalse);
        void ClearByteCollection();

        void CopyRunOpt(int inum);

        void RunOptNum(int inum);
        void RunOptNum_TimeLimit(int inum);

        void SetRunOpt(const string &strname);
        void RunOptString(const char *a_szname);

        void ClearOptMap();
        void ResetRun();
        //void SetOptLimitTime(double dlimittime);
        //double GetOptRunTime();

    private:
        std::ostream *m_stream;
        std::ostream *m_createstream;
        std::vector<std::string> m_collectedScriptStatements;
//////////////////////////////////////////////////////////////////////////
    public:
        void CreateClassDef(const char *pclassname,
                            const char *pclassdef);
        void CreateClassFunc(const char *pclassname,
            const char *pclassfucname,
            const char *pclassfucdef);
        void ListCreateClassDef(mu::Parser &Pparser
            ,const char *pclassname);
        void ListCreateClassFunDef(mu::Parser &Pparser
            ,const char *pclassname
            ,const char *pclassfuncname);

        void ListCreateClassDef(const char *pclassname);
        void ListCreateClassFunDef(const char *pclassname
            ,const char *pclassfuncname);
//////////////////////////////////////////////////////////////////////////
        bool IsObject(mu::Parser &Pparser,const char *a_szClassObject);
        bool IsObjectVar(const char *a_sz);
        void GetImageObjectAutoSave();
//////////////////////////////////////////////////////////////////////////
        classbase *m_pdoubleclass;
    };
}
#endif //PARSERCLASS_H
