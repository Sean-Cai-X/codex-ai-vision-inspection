

#ifndef MU_PARSER_CLASS_H
#define MU_PARSER_CLASS_H

#include "muParserDef.h"
#include<vector>
#include<typeinfo>
using namespace std;

namespace mu
{
        class ParserBase;

        typedef std::vector<double> paramvect;
        typedef std::vector<void *> voidparamvect;
        typedef std::vector<string> charpvect;

        
        enum ClassFunReturnKind
        {
                ClassFunReturnVoid = 0,
                ClassFunReturnInt,
                ClassFunReturnDouble,
                ClassFunReturnCharp
        };

        enum ClassFunParamShape
        {
                ClassFunParamUnknown = 0,
                ClassFunParamNone,
                ClassFunParamFixedDouble,
                ClassFunParamFixedInt,
                ClassFunParamFixedCharp,
                ClassFunParamFixedDoubleCharp,
                ClassFunParamFixedVoidp,
                ClassFunParamAnyNumeric,
                ClassFunParamAnyCharp
        };

        struct ClassFunSignatureShape
        {
                ClassFunParamShape param_shape;
                ClassFunReturnKind return_kind;
                bool variadic;
        };

        inline ClassFunSignatureShape DescribeClassFunSignature(int param_type)
        {
                switch(param_type)
                {
                case Param_none:
                        return {ClassFunParamNone, ClassFunReturnVoid, false};
                case Param_voidp_1:
                        return {ClassFunParamFixedVoidp, ClassFunReturnVoid, false};
                case Param_voidp_1_Return_double:
                        return {ClassFunParamFixedVoidp, ClassFunReturnDouble, false};
                case Param_double_1:
                case Param_double_2:
                case Param_double_3:
                case Param_double_4:
                        return {ClassFunParamFixedDouble, ClassFunReturnVoid, false};
                case Param_int_1:
                case Param_int_2:
                case Param_int_3:
                case Param_int_4:
                case Param_int_5:
                case Param_int_6:
                case Param_int_7:
                        return {ClassFunParamFixedInt, ClassFunReturnVoid, false};
                case Param_charp_1:
                        return {ClassFunParamFixedCharp, ClassFunReturnVoid, false};
                case Param_double_charp_2:
                        return {ClassFunParamFixedDoubleCharp, ClassFunReturnVoid, false};
                case Param_double_charp_2_Return_double:
                        return {ClassFunParamFixedDoubleCharp, ClassFunReturnDouble, false};
                case Param_int_double_2:
                case Param_double_int_2:
                        return {ClassFunParamFixedDouble, ClassFunReturnVoid, false};
                case Param_int_double_2_Return_int:
                case Param_double_int_2_Return_int:
                        return {ClassFunParamFixedDouble, ClassFunReturnInt, false};
                case Param_int_double_2_Return_double:
                case Param_double_int_2_Return_double:
                        return {ClassFunParamFixedDouble, ClassFunReturnDouble, false};
                case Param_charp_any:
                        return {ClassFunParamAnyCharp, ClassFunReturnVoid, true};
                case Param_charp_any_Return_int:
                        return {ClassFunParamAnyCharp, ClassFunReturnInt, true};
                case Param_charp_any_Return_double:
                        return {ClassFunParamAnyCharp, ClassFunReturnDouble, true};
                case Param_0_Return_charp:
                        return {ClassFunParamNone, ClassFunReturnCharp, false};
                case Param_0_Return_int:
                        return {ClassFunParamNone, ClassFunReturnInt, false};
                case Param_0_Return_double:
                        return {ClassFunParamNone, ClassFunReturnDouble, false};
                case Param_int_1_Return_int:
                case Param_int_2_Return_int:
                        return {ClassFunParamFixedInt, ClassFunReturnInt, false};
                case Param_int_1_Return_double:
                case Param_int_2_Return_double:
                case Param_int_3_Return_double:
                        return {ClassFunParamFixedInt, ClassFunReturnDouble, false};
                case Param_any:
                        return {ClassFunParamAnyNumeric, ClassFunReturnVoid, true};
                case Param_any_Return_int:
                        return {ClassFunParamAnyNumeric, ClassFunReturnInt, true};
                case Param_any_Return_double:
                        return {ClassFunParamAnyNumeric, ClassFunReturnDouble, true};
                default:
                        return {ClassFunParamUnknown, ClassFunReturnVoid, false};
                }
        }

        class classbasefunc
        {
        public:
                virtual int GetArgCount()
                {
                        return 0;
                }
                virtual int GetArgType()
                {
                        return 0;
                }

                virtual void UseFUNC()
                {
                };
                virtual double UseFUNC_Return()
                {
                        return 0;
                }
                virtual char * UseFUNC_ReturnChar()
                {
                        return 0;
                }
                virtual void SetClassObj(void *pclass)
                {
                        (void)pclass;
                }
                virtual void AddParam(double dparm)
                {
                        (void)dparm;
                }
                virtual void AddParam(paramvect &avect)
                {
                        (void)avect;
                }
                virtual void ClearvoidpParam()
                {

                }
                virtual void AddVoidpParam(voidparamvect &avect)
                {
                        (void)avect;
                }
                virtual void AddCharpParam(charpvect &charPparm)
                {
                        (void)charPparm;
                };

        };

        template<class ACLASS>
        class classfuncstorage: public classbasefunc
        {
                int m_ifunctype;

                typedef void (ACLASS::*PDF0)(void );
                typedef void (ACLASS::*PDFVOIDP)(void* );
                typedef double (ACLASS::*PDFVOIDP_R_D)(void* );
                typedef void (ACLASS::*PDF1)(double );
                typedef void (ACLASS::*PDF2)(double,double );
                typedef void (ACLASS::*PDF3)(double,double,double);
                typedef void (ACLASS::*PDF4)(double,double,double,double);
                typedef void (ACLASS::*PIF1)(int );
                typedef void (ACLASS::*PIF2)(int,int );
                typedef void (ACLASS::*PIF3)(int,int,int);
                typedef void (ACLASS::*PIF4)(int,int,int,int);
                typedef void (ACLASS::*PIF5)(int,int,int,int,int);
                typedef void (ACLASS::*PIF6)(int,int,int,int,int,int);
                typedef void (ACLASS::*PIF7)(int,int,int,int,int,int,int);
                typedef int (ACLASS::*PF0_R_I)();
                typedef double (ACLASS::*PF0_R_D)();
                typedef int (ACLASS::*PIF1_R_I)(int);
                typedef int (ACLASS::*PIF2_R_I)(int,int);
                typedef double (ACLASS::*PIF1_R_D)(int );
                typedef double (ACLASS::*PIF2_R_D)(int,int);
                typedef double (ACLASS::*PIF3_R_D)(int,int,int);
                typedef void (ACLASS::*PIFD2)(int,double);
                typedef void (ACLASS::*PDFI2)(double,int);
                typedef int (ACLASS::*PIFD2_R_I)(int,double);
                typedef int (ACLASS::*PDFI2_R_I)(double,int);
                typedef double (ACLASS::*PIFD2_R_D)(int,double);
                typedef double (ACLASS::*PDFI2_R_D)(double,int);

                typedef void (ACLASS::*PCF1)(const char_type *);
                typedef void (ACLASS::*PDFC2)(double, const char_type *);
                typedef void (ACLASS::*PCF_ANY)(charpvect &);
                typedef void (ACLASS::*PANY)(paramvect &);
                typedef int (ACLASS::*PCF_ANY_R_I)(charpvect &);
                typedef double (ACLASS::*PDFC2_R_D)(double, const char_type *);
                typedef double (ACLASS::*PCF_ANY_R_D)(charpvect &);
                typedef int (ACLASS::*PANY_R_I)(paramvect &);
                typedef double (ACLASS::*PANY_R_D)(paramvect &);

                typedef char_type *(ACLASS::*PF0_R_C)();

                PDF0 m_pdf0;
                PDFVOIDP m_pdfvoidp;
                PDFVOIDP_R_D m_pdfvoidp_re_double;

                PDF1 m_pdf1;
                PDF2 m_pdf2;
                PDF3 m_pdf3;
                PDF4 m_pdf4;
                PIF1 m_pif1;
                PIF2 m_pif2;
                PIF3 m_pif3;
                PIF4 m_pif4;
                PIF5 m_pif5;
                PIF6 m_pif6;
                PIF7 m_pif7;

                PF0_R_I m_pf_0_re_int;
                PF0_R_D m_pf_0_re_double;

                PIF1_R_I m_pif1_re_int;
                PIF2_R_I m_pif2_re_int;
                PIF1_R_D m_pif1_re_double;
                PIF2_R_D m_pif2_re_double;
                PIF3_R_D m_pif3_re_double;
                PIFD2 m_pifd2;
                PDFI2 m_pdfi2;
                PIFD2_R_I m_pifd2_re_int;
                PDFI2_R_I m_pdfi2_re_int;
                PIFD2_R_D m_pifd2_re_double;
                PDFI2_R_D m_pdfi2_re_double;

                PCF1 m_pfc1;
                PDFC2 m_pdfc2;
                PCF_ANY m_pfc_any;
                PANY m_pany;
                PCF_ANY_R_I m_pfc_any_re_int;
                PDFC2_R_D m_pdfc2_re_double;
                PCF_ANY_R_D m_pfc_any_re_double;
                PANY_R_I m_pany_re_int;
                PANY_R_D m_pany_re_double;

                PF0_R_C m_pf0_re_charp;

                ACLASS *m_pclass;

                paramvect m_parm;

                voidparamvect m_voidPparm;

                charpvect m_charPparm;
        public:

                virtual int GetArgCount()
                {
                        switch(m_ifunctype)
                        {
                        case Param_none:
                                return 0;

                        case Param_voidp_1:
                                return 1;
                        case Param_voidp_1_Return_double:
                                return 1;

                        case Param_double_1:
                                return 1;
                        case Param_double_2:
                                return 2;
                        case Param_double_3:
                                return 3;
                        case Param_double_4:
                                return 4;

                        case Param_int_1:
                                return 1;
                        case Param_int_2:
                                return 2;
                        case Param_int_3:
                                return 3;
                        case Param_int_4:
                                return 4;
                        case Param_int_5:
                                return 5;
                        case Param_int_6:
                                return 6;
                        case Param_int_7:
                                return 7;

                        case Param_charp_1:
                                return 1;
                        case Param_double_charp_2:
                                return 2;
                        case Param_double_charp_2_Return_double:
                                return 2;
                        case Param_int_double_2:
                        case Param_double_int_2:
                        case Param_int_double_2_Return_int:
                        case Param_double_int_2_Return_int:
                        case Param_int_double_2_Return_double:
                        case Param_double_int_2_Return_double:
                                return 2;
                        case Param_charp_any:
                                return -1;
                        case Param_charp_any_Return_int:
                                return -1;
                        case Param_charp_any_Return_double:
                                return -1;

                        case Param_0_Return_charp:
                                return 0;

                        case Param_0_Return_int:
                                return 0;

                        case Param_0_Return_double:
                                return 0;

                        case Param_int_1_Return_int:
                                return 1;

                        case Param_int_2_Return_int:
                                return 2;

                        case Param_int_1_Return_double:
                                return 1;

                        case Param_int_2_Return_double:
                                return 2;
                        case Param_int_3_Return_double:
                                return 3;
                        case Param_any:
                                return -1;
                        case Param_any_Return_int:
                                return -1;
                        case Param_any_Return_double:
                                return -1;
                        default :
                                return -1;
                        }

                }
                virtual int GetArgType()
                {
                        return m_ifunctype;
                }

                virtual void UseFUNC()
                {
                        switch(m_ifunctype)
                        {
                        case Param_none:
                                (m_pclass->*m_pdf0)();
                                break;
                        case Param_voidp_1:
                                if(m_voidPparm.size()>=1)
                                        (m_pclass->*m_pdfvoidp)(m_voidPparm[0]);
                                break;
                        case Param_double_1:
                                if(m_parm.size()>=1)
                                        (m_pclass->*m_pdf1)(m_parm[0]);
                                break;
                        case Param_double_2:
                                if(m_parm.size()>=2)
                                        (m_pclass->*m_pdf2)(m_parm[1],m_parm[0]);
                                break;
                        case Param_double_3:
                                if(m_parm.size()>=3)
                                        (m_pclass->*m_pdf3)(m_parm[2],m_parm[1],m_parm[0]);
                                break;
                        case Param_double_4:
                                if(m_parm.size()>=4)
                                        (m_pclass->*m_pdf4)(m_parm[3],m_parm[2],m_parm[1],m_parm[0]);
                                break;

                        case Param_int_1:
                                if(m_parm.size()>=1)
                                        (m_pclass->*m_pif1)(static_cast<int>(m_parm[0]));
                                break;
                        case Param_int_2:
                                if(m_parm.size()>=2)
                                        (m_pclass->*m_pif2)(static_cast<int>(m_parm[1]),static_cast<int>(m_parm[0]));
                                break;
                        case Param_int_double_2:
                                if(m_parm.size()>=2)
                                        (m_pclass->*m_pifd2)(static_cast<int>(m_parm[1]),static_cast<double>(m_parm[0]));
                                break;
                        case Param_double_int_2:
                                if(m_parm.size()>=2)
                                        (m_pclass->*m_pdfi2)(static_cast<double>(m_parm[1]),static_cast<int>(m_parm[0]));
                                break;
                        case Param_int_3:
                                if(m_parm.size()>=3)
                                        (m_pclass->*m_pif3)(static_cast<int>(m_parm[2]),static_cast<int>(m_parm[1]),static_cast<int>(m_parm[0]));
                                break;
                        case Param_int_4:
                                if(m_parm.size()>=4)
                                        (m_pclass->*m_pif4)(static_cast<int>(m_parm[3]),static_cast<int>(m_parm[2]),static_cast<int>(m_parm[1]),static_cast<int>(m_parm[0]));
                                break;
                        case Param_int_5:
                                if(m_parm.size()>=5)
                                        (m_pclass->*m_pif5)(static_cast<int>(m_parm[4]),static_cast<int>(m_parm[3]),static_cast<int>(m_parm[2]),static_cast<int>(m_parm[1]),static_cast<int>(m_parm[0]));
                                break;
                        case Param_int_6:
                                if(m_parm.size()>=6)
                                        (m_pclass->*m_pif6)(static_cast<int>(m_parm[5]),static_cast<int>(m_parm[4]),static_cast<int>(m_parm[3]),static_cast<int>(m_parm[2]),static_cast<int>(m_parm[1]),static_cast<int>(m_parm[0]));
                                break;
                        case Param_int_7:
                                if(m_parm.size()>=7)
                                        (m_pclass->*m_pif7)(static_cast<int>(m_parm[6]),static_cast<int>(m_parm[5]),static_cast<int>(m_parm[4]),static_cast<int>(m_parm[3]),static_cast<int>(m_parm[2]),static_cast<int>(m_parm[1]),static_cast<int>(m_parm[0]));
                                break;

                        case Param_charp_1:
                                if(m_charPparm.size()>=1)
                                        (m_pclass->*m_pfc1)(m_charPparm[0].c_str());
                                break;
                        case Param_double_charp_2:
                                if(m_parm.size()>=1 && m_charPparm.size()>=1)
                                        (m_pclass->*m_pdfc2)(m_parm[0], m_charPparm[0].c_str());
                                break;
                        case Param_charp_any:
                                (m_pclass->*m_pfc_any)(m_charPparm);
                                break;
                        case Param_any:
                                (m_pclass->*m_pany)(m_parm);
                                break;
                        case Param_charp_any_Return_int:
                                (m_pclass->*m_pfc_any_re_int)(m_charPparm);
                                break;
                        case Param_charp_any_Return_double:
                                (m_pclass->*m_pfc_any_re_double)(m_charPparm);
                                break;
                        case Param_any_Return_int:
                                (m_pclass->*m_pany_re_int)(m_parm);
                                break;
                        case Param_any_Return_double:
                                (m_pclass->*m_pany_re_double)(m_parm);
                                break;
                                default :
                                        break;
                        }
                }

                virtual double UseFUNC_Return()
                {
                        switch(m_ifunctype)
                        {

                        case Param_none:
                                (m_pclass->*m_pdf0)();
                                break;
                        case Param_voidp_1:
                                if(m_voidPparm.size()>=1)
                                        (m_pclass->*m_pdfvoidp)(m_voidPparm[0]);
                                break;
                        case Param_double_1:
                                if(m_parm.size()>=1)
                                        (m_pclass->*m_pdf1)(m_parm[0]);
                                break;
                        case Param_double_2:
                                if(m_parm.size()>=2)
                                        (m_pclass->*m_pdf2)(m_parm[1],m_parm[0]);
                                break;
                        case Param_double_3:
                                if(m_parm.size()>=3)
                                        (m_pclass->*m_pdf3)(m_parm[2],m_parm[1],m_parm[0]);
                                break;
                        case Param_double_4:
                                if(m_parm.size()>=4)
                                        (m_pclass->*m_pdf4)(m_parm[3],m_parm[2],m_parm[1],m_parm[0]);
                                break;

                        case Param_int_1:
                                if(m_parm.size()>=1)
                                        (m_pclass->*m_pif1)(static_cast<int>(m_parm[0]));
                                break;
                        case Param_int_2:
                                if(m_parm.size()>=2)
                                        (m_pclass->*m_pif2)(static_cast<int>(m_parm[1]),static_cast<int>(m_parm[0]));
                                break;
                        case Param_int_3:
                                if(m_parm.size()>=3)
                                        (m_pclass->*m_pif3)(static_cast<int>(m_parm[2]),static_cast<int>(m_parm[1]),static_cast<int>(m_parm[0]));
                                break;
                        case Param_int_4:
                                if(m_parm.size()>=4)
                                        (m_pclass->*m_pif4)(static_cast<int>(m_parm[3]),static_cast<int>(m_parm[2]),static_cast<int>(m_parm[1]),static_cast<int>(m_parm[0]));
                                break;
                        case Param_int_5:
                                if(m_parm.size()>=5)
                                        (m_pclass->*m_pif5)(static_cast<int>(m_parm[4]),static_cast<int>(m_parm[3]),static_cast<int>(m_parm[2]),static_cast<int>(m_parm[1]),static_cast<int>(m_parm[0]));
                                break;
                        case Param_int_6:
                                if(m_parm.size()>=6)
                                        (m_pclass->*m_pif6)(static_cast<int>(m_parm[5]),static_cast<int>(m_parm[4]),static_cast<int>(m_parm[3]),static_cast<int>(m_parm[2]),static_cast<int>(m_parm[1]),static_cast<int>(m_parm[0]));
                                break;
                        case Param_int_7:
                                if(m_parm.size()>=7)
                                        (m_pclass->*m_pif7)(static_cast<int>(m_parm[6]),static_cast<int>(m_parm[5]),static_cast<int>(m_parm[4]),static_cast<int>(m_parm[3]),static_cast<int>(m_parm[2]),static_cast<int>(m_parm[1]),static_cast<int>(m_parm[0]));
                                break;

                        case Param_charp_1:
                                if(m_charPparm.size()>=1)
                                        (m_pclass->*m_pfc1)(m_charPparm[0].c_str());
                                break;
                        case Param_double_charp_2:
                                if(m_parm.size()>=1 && m_charPparm.size()>=1)
                                        (m_pclass->*m_pdfc2)(m_parm[0], m_charPparm[0].c_str());
                                break;
                        case Param_double_charp_2_Return_double:
                                if(m_parm.size()>=1 && m_charPparm.size()>=1)
                                        return (m_pclass->*m_pdfc2_re_double)(m_parm[0], m_charPparm[0].c_str());
                                break;
                        case Param_charp_any:
                                (m_pclass->*m_pfc_any)(m_charPparm);
                                break;
                        case Param_charp_any_Return_int:
                                return (m_pclass->*m_pfc_any_re_int)(m_charPparm);
                                break;
                        case Param_charp_any_Return_double:
                                return (m_pclass->*m_pfc_any_re_double)(m_charPparm);
                                break;

                        case Param_any:
                                (m_pclass->*m_pany)(m_parm);
                                break;
                        case Param_any_Return_int:
                                return (m_pclass->*m_pany_re_int)(m_parm);
                                break;
                        case Param_any_Return_double:
                                return (m_pclass->*m_pany_re_double)(m_parm);
                                break;

                        case Param_voidp_1_Return_double:
                                if(m_voidPparm.size()>=1)
                                        return (m_pclass->*m_pdfvoidp_re_double)(m_voidPparm[0]);
                                break;
                        case Param_0_Return_int:
                                        return (m_pclass->*m_pf_0_re_int)();
                                break;
                        case Param_0_Return_double:
                                        return (m_pclass->*m_pf_0_re_double)();
                                break;
                        case Param_int_1_Return_int:
                                if(m_parm.size()>=1)
                                        return (m_pclass->*m_pif1_re_int)(m_parm[0]);
                                break;
                        case Param_int_2_Return_int:
                                if(m_parm.size()>=2)
                                        return (m_pclass->*m_pif2_re_int)(m_parm[1],m_parm[0]);
                        case Param_int_double_2_Return_int:
                                if(m_parm.size()>=2)
                                        return (m_pclass->*m_pifd2_re_int)(static_cast<int>(m_parm[1]),static_cast<double>(m_parm[0]));
                                break;
                        case Param_double_int_2_Return_int:
                                if(m_parm.size()>=2)
                                        return (m_pclass->*m_pdfi2_re_int)(static_cast<double>(m_parm[1]),static_cast<int>(m_parm[0]));
                                break;

                        case Param_int_1_Return_double:
                                if(m_parm.size()>=1)
                                        return (m_pclass->*m_pif1_re_double)(m_parm[0]);
                                break;
                        case Param_int_2_Return_double:
                                if(m_parm.size()>=2)
                                        return (m_pclass->*m_pif2_re_double)(m_parm[1],m_parm[0]);
                                break;
                        case Param_int_3_Return_double:
                                if(m_parm.size()>=3)
                                        return (m_pclass->*m_pif3_re_double)(m_parm[2],m_parm[1],m_parm[0]);
                                break;
                        case Param_int_double_2_Return_double:
                                if(m_parm.size()>=2)
                                        return (m_pclass->*m_pifd2_re_double)(static_cast<int>(m_parm[1]),static_cast<double>(m_parm[0]));
                                break;
                        case Param_double_int_2_Return_double:
                                if(m_parm.size()>=2)
                                        return (m_pclass->*m_pdfi2_re_double)(static_cast<double>(m_parm[1]),static_cast<int>(m_parm[0]));
                                break;
                        default :
                                break;
                        }
                        return 0;
                }
                virtual char * UseFUNC_ReturnChar()
                {

                        switch(m_ifunctype)
                        {
                                        case Param_0_Return_charp:
                                                return (m_pclass->*m_pf0_re_charp)();
                                                break;

                                        default :
                                                break;
                        }
                        return 0;

                }
                virtual void SetClassObj(void *pclass)
                {
                        m_pclass=(ACLASS *)pclass;
                }
                void StorageFUNC(void (ACLASS::*pdf)(void ))
                {
                        m_pdf0=pdf;
                        m_ifunctype = Param_none ;
                }
                void StorageFUNC(void (ACLASS::*pdfvoidp)(void* ))
                {
                        m_pdfvoidp=pdfvoidp;
                        m_ifunctype = Param_voidp_1 ;
                }
                void StorageFUNC(double (ACLASS::*pdfvoidp_re_double)(void* ))
                {
                        m_pdfvoidp_re_double=pdfvoidp_re_double;
                        m_ifunctype = Param_voidp_1_Return_double ;
                }
                void StorageFUNC(void (ACLASS::*pdf1)(double ))
                {
                        m_pdf1=pdf1;
                        m_ifunctype = Param_double_1 ;
                }
                void StorageFUNC(void (ACLASS::*pdf2)(double,double ))
                {
                        m_pdf2=pdf2;
                        m_ifunctype = Param_double_2 ;
                }
                void StorageFUNC(void (ACLASS::*pdf3)(double,double,double))
                {
                        m_pdf3=pdf3;
                        m_ifunctype = Param_double_3 ;
                }
                void StorageFUNC(void (ACLASS::*pdf4)(double,double,double,double))
                {
                        m_pdf4=pdf4;
                        m_ifunctype = Param_double_4 ;
                }
                void StorageFUNC(void (ACLASS::*pif1)(int))
                {
                        m_pif1=pif1;
                        m_ifunctype = Param_int_1 ;
                }
                void StorageFUNC(void (ACLASS::*pif2)(int,int))
                {
                        m_pif2=pif2;
                        m_ifunctype = Param_int_2 ;
                }
                void StorageFUNC(void (ACLASS::*pifd2)(int,double))
                {
                        m_pifd2=pifd2;
                        m_ifunctype = Param_int_double_2;
                }
                void StorageFUNC(void (ACLASS::*pdfi2)(double,int))
                {
                        m_pdfi2=pdfi2;
                        m_ifunctype = Param_double_int_2;
                }
                void StorageFUNC(void (ACLASS::*pif3)(int,int,int))
                {
                        m_pif3=pif3;
                        m_ifunctype = Param_int_3 ;
                }
                void StorageFUNC(void (ACLASS::*pif4)(int,int,int,int))
                {
                        m_pif4=pif4;
                        m_ifunctype = Param_int_4 ;
                }
                void StorageFUNC(void (ACLASS::*pif5)(int,int,int,int,int))
                {
                        m_pif5=pif5;
                        m_ifunctype = Param_int_5 ;
                }
                void StorageFUNC(void (ACLASS::*pif6)(int,int,int,int,int,int))
                {
                        m_pif6=pif6;
                        m_ifunctype = Param_int_6 ;
                }

                void StorageFUNC(void (ACLASS::*pif7)(int,int,int,int,int,int,int))
                {
                        m_pif7=pif7;
                        m_ifunctype = Param_int_7 ;
                }

                void StorageFUNC(void (ACLASS::*pfc1)(const char_type *))
                {
                        m_pfc1=pfc1;
                        m_ifunctype = Param_charp_1 ;
                }
                void StorageFUNC(void (ACLASS::*pdfc2)(double,const char_type *))
                {
                        m_pdfc2=pdfc2;
                        m_ifunctype = Param_double_charp_2;
                }
                void StorageFUNC(void (ACLASS::*pfc_any)(charpvect &))
                {
                        m_pfc_any=pfc_any;
                        m_ifunctype = Param_charp_any;
                }
                void StorageFUNC(void (ACLASS::*pany)(paramvect &))
                {
                        m_pany=pany;
                        m_ifunctype = Param_any ;
                }
                void StorageFUNC(char* (ACLASS::*pf0_re_c)())
                {
                        m_pf0_re_charp=pf0_re_c;
                        m_ifunctype = Param_0_Return_charp ;
                }
                void StorageFUNC(int (ACLASS::*pf0_re_int)())
                {
                        m_pf_0_re_int=pf0_re_int;
                        m_ifunctype = Param_0_Return_int ;
                }

                void StorageFUNC(double (ACLASS::*pf0_re_double)())
                {
                        m_pf_0_re_double=pf0_re_double;
                        m_ifunctype = Param_0_Return_double ;
                }

                void StorageFUNC(int (ACLASS::*pif1_re_int)(int))
                {
                        m_pif1_re_int=pif1_re_int;
                        m_ifunctype = Param_int_1_Return_int ;
                }
                void StorageFUNC(int (ACLASS::*pif2_re_int)(int,int))
                {
                        m_pif2_re_int=pif2_re_int;
                        m_ifunctype = Param_int_2_Return_int ;
                }
                void StorageFUNC(int (ACLASS::*pifd2_re_int)(int,double))
                {
                        m_pifd2_re_int=pifd2_re_int;
                        m_ifunctype = Param_int_double_2_Return_int;
                }
                void StorageFUNC(int (ACLASS::*pdfi2_re_int)(double,int))
                {
                        m_pdfi2_re_int=pdfi2_re_int;
                        m_ifunctype = Param_double_int_2_Return_int;
                }
                void StorageFUNC(double (ACLASS::*pif1_re_db)(int))
                {
                        m_pif1_re_double=pif1_re_db;
                        m_ifunctype = Param_int_1_Return_double ;
                }
                void StorageFUNC(double (ACLASS::*pif2_re_db)(int,int))
                {
                        m_pif2_re_double=pif2_re_db;
                        m_ifunctype = Param_int_2_Return_double ;
                }
                void StorageFUNC(double (ACLASS::*pifd2_re_db)(int,double))
                {
                        m_pifd2_re_double=pifd2_re_db;
                        m_ifunctype = Param_int_double_2_Return_double;
                }
                void StorageFUNC(double (ACLASS::*pdfi2_re_db)(double,int))
                {
                        m_pdfi2_re_double=pdfi2_re_db;
                        m_ifunctype = Param_double_int_2_Return_double;
                }
                void StorageFUNC(double (ACLASS::*pif3_re_db)(int,int,int))
                {
                        m_pif3_re_double=pif3_re_db;
                        m_ifunctype = Param_int_3_Return_double ;
                }
                void StorageFUNC(int (ACLASS::*pany_re_int)(paramvect &))
                {
                        m_pany_re_int=pany_re_int;
                        m_ifunctype = Param_any_Return_int ;
                }
                void StorageFUNC(int (ACLASS::*pfc_any_re_int)(charpvect &))
                {
                        m_pfc_any_re_int=pfc_any_re_int;
                        m_ifunctype = Param_charp_any_Return_int;
                }
                void StorageFUNC(double (ACLASS::*pany_re_double)(paramvect &))
                {
                        m_pany_re_double=pany_re_double;
                        m_ifunctype = Param_any_Return_double ;
                }
                void StorageFUNC(double (ACLASS::*pdfc2_re_double)(double,const char_type *))
                {
                        m_pdfc2_re_double=pdfc2_re_double;
                        m_ifunctype = Param_double_charp_2_Return_double;
                }
                void StorageFUNC(double (ACLASS::*pfc_any_re_double)(charpvect &))
                {
                        m_pfc_any_re_double=pfc_any_re_double;
                        m_ifunctype = Param_charp_any_Return_double;
                }
                virtual void AddParam(double dparm)
                {
                        m_parm.push_back(dparm);
                }
                virtual void AddParam(paramvect &avect)
                {
                        m_parm	=	avect;
                }
                virtual void AddVoidpParam(voidparamvect &avect)
                {
                        m_voidPparm = avect;
                }
                virtual void AddCharpParam(charpvect &charPparm)
                {
                        m_charPparm = charPparm;
                }

                virtual void ClearParam()
                {
                        m_parm.clear();
                }
                virtual void ClearvoidpParam()
                {
                        m_voidPparm.clear();
                }
                virtual void ClearCharpParam()
                {
                        m_charPparm.clear();
                }

        };
        typedef std::vector<classbasefunc*> basefuncvector;

        class classbase
        {
        public:
                int m_iclasstype;
        public:
                classbase(){};
                virtual ~classbase()
                {

                };
                void deleteself(){delete this;};
        public:
                virtual void * addvar(const string_type & strname)
                {
                        (void)strname;
                        return 0;
                };
                virtual int findstacknum(void *pclassobj)
                {
                        (void)pclassobj;
                        return -1;
                };
                virtual void * getstackvar(int inum)
                {
                        (void)inum;
                        return 0;
                };
                virtual void clearvar()
                {

                }
                virtual void delvar(void *pobj)
                {
                        (void)pobj;
                }
                virtual void delvar(const string_type& strname)
                {
                        (void)strname;
                }
                virtual void* addpointvar(const string_type & strname,void *pclassobject)
                {
                        (void)strname;
                        (void)pclassobject;
                        return 0;
                }

                virtual bool findvar(const string_type & a_strVarName)
                {
                        (void)a_strVarName;
                        return false;
                };
                virtual const char*   getclass() const
                {
                        return  string_type("null").c_str();
                };
                virtual void * getvar(const string_type & a_str)
                {
                        (void)a_str;
                        return 0;
                };
                virtual const char* getvar(int a_num)
                {
                        (void)a_num;
                        return string_type("none").c_str();
                }
                virtual void * getvarpoint(int a_num)
                {
                        (void)a_num;
                        return 0;
                }
                virtual void * getvarpoint(const string_type & a_str)
                {
                        (void)a_str;
                        return 0;
                }
                virtual const char* getpointvar(int a_num)
                {
                        (void)a_num;
                        return string_type("none").c_str();
                }
                virtual void * getpointvarp(int a_num)
                {
                        (void)a_num;
                        return 0;
                }
                virtual void Setpointvarp(const string_type & a_str,void *pvar)
                {
                        (void)a_str;
                        (void)pvar;
                };

                virtual int size()
                {
                        return  0;
                }
                virtual int pointsize()
                {
                        return  0;
                }
                virtual void AddClassFun(const string_type &a_strName, void* a_pFun)
                {
                        (void)a_strName;
                        (void)a_pFun;
                        return;
                }
                virtual bool findclassfun(const string_type &a_strName)
                {
                        (void)a_strName;
                        return false;
                }
                virtual double ApplyClassFunc(void *pobj,const string_type &a_strName,paramvect& parm)
                {
                        (void)pobj;
                        (void)a_strName;
                        (void)parm;
                        return 0;
                };
                virtual void* GetClassFuncLP(const string_type &strname)
                {
                        (void)strname;
                        return 0;
                }
                virtual double ApplyClassFunc(void *pobj,const string_type &a_strName,voidparamvect& parm)
                {
                        (void)pobj;
                        (void)a_strName;
                        (void)parm;
                        return 0;
                };
                virtual double ApplyClassFunc(void *pobj,const string_type &a_strName,charpvect& parm)
                {
                        (void)pobj;
                        (void)a_strName;
                        (void)parm;
                        return 0;
                };
                virtual double ApplyClassFunc(void *pobj,const string_type &a_strName,paramvect& parm,charpvect& charparm)
                {
                        (void)pobj;
                        (void)a_strName;
                        (void)parm;
                        (void)charparm;
                        return 0;
                };

                virtual double ApplyClassFunc(void *pobj,void  *apclassfunc,paramvect& parm)
                {
                        (void)pobj;
                        (void)apclassfunc;
                        (void)parm;
                        return 0;
                };

                virtual double ApplyClassFunc(void *pobj,void  *apclassfunc,voidparamvect& parm)
                {
                        (void)pobj;
                        (void)apclassfunc;
                        (void)parm;
                        return 0;
                };
                virtual char* ApplyClassFuncString(void *pobj,const string_type &a_strName)
                {
                        (void)pobj;
                        (void)a_strName;
                        return 0;
                };

                virtual double ApplyClassFunc(void *pobj,void  *apclassfunc,charpvect& parm)
                {
                        (void)pobj;
                        (void)apclassfunc;
                        (void)parm;
                        return 0;
                };
                virtual double ApplyClassFunc(void *pobj,void  *apclassfunc,paramvect& parm,charpvect& charparm)
                {
                        (void)pobj;
                        (void)apclassfunc;
                        (void)parm;
                        (void)charparm;
                        return 0;
                };
                virtual int GetFuncArgType(const string_type &a_strName)
                {
                        (void)a_strName;
                        return 0;
                }
                virtual ClassFunSignatureShape GetFuncSignatureShape(const string_type &a_strName)
                {
                        return DescribeClassFunSignature(GetFuncArgType(a_strName));
                }
                virtual int GetFuncArgCount(const string_type &a_strName)
                {
                        (void)a_strName;
                        return 0;
                }
                virtual const char* getfuncname(int a_num)
                {
                        (void)a_num;
                        return string_type("none").c_str();
                }
                virtual const char* getfunctype(int a_num)
                {
                        (void)a_num;
                        return string_type("none").c_str();
                }
                virtual int funcsize()
                {
                        return 0;
                }
                virtual classbase*GetMe()
                {
                        return this;
                }

                virtual bool Iscreateclass()
                {
                        return false;
                }

        };

        template<class TCLASS>
        class OrgClass:public classbase
        {
                typedef std::map<string_type,TCLASS*> OrgStorage_type;
                typedef std::vector<TCLASS*> OrgVect;
                OrgStorage_type m_ObjectStorage;
                OrgVect m_Stack;

                TCLASS *m_p;
        public:
                OrgClass()
                {
                        m_iclasstype = CLASS_ORG;
                }
                virtual ~OrgClass()
                {
                        typename OrgStorage_type:: iterator pIter;
                        for ( pIter = m_ObjectStorage.begin( ) ; pIter != m_ObjectStorage.end( ) ; pIter++ )
                        {
                                TCLASS *pbase=pIter->second;
                                delete pbase;
                                pIter->second=0;
                        }
                };
                void deleteself(){delete this;};

                virtual void * addvar(const string_type & strname)
                {
                        typename OrgStorage_type::iterator item = m_ObjectStorage.find(strname);
                        if (item!=m_ObjectStorage.end())
                                return 0;
                        TCLASS *apclass=new TCLASS;
                        m_ObjectStorage[strname]=apclass;
                        m_Stack.push_back(apclass);
                        return apclass;
                };
                virtual int findstacknum(void *pclassobj)
                {
                        typename OrgVect::iterator pIter;
                        int inum = 0;
                        for(pIter = m_Stack.begin();pIter != m_Stack.end() ; pIter++)
                        {
                                TCLASS *pbase = *pIter;
                                if(pbase == pclassobj)
                                        return inum;
                                inum ++;
                        }
                        return -1;
                };
                virtual void clearvar()
                {
                        typename OrgStorage_type:: iterator pIter;
                        for ( pIter = m_ObjectStorage.begin( ) ; pIter != m_ObjectStorage.end( ) ; pIter++ )
                        {
                                TCLASS *pbase=pIter->second;
                                delete pbase;
                                pIter->second=0;
                        }
                        m_ObjectStorage.clear();
                }
                virtual void delvar(void *pobj)
                {
                        typename OrgStorage_type:: iterator pIter;
                        TCLASS *pbase;
                        for ( pIter = m_ObjectStorage.begin( ) ; pIter != m_ObjectStorage.end( ) ; pIter++ )
                        {
                                pbase=pIter->second;
                                if(pobj==pbase)
                                {
                                        delete pbase;
                                        m_ObjectStorage.erase(pIter);
                                        return;
                                }
                        }
                }
                virtual void delvar(const string_type& strname)
                {
                        typename OrgStorage_type:: iterator pIter;
                        TCLASS *pbase;
                        string_type strget;
                        for ( pIter = m_ObjectStorage.begin( ) ; pIter != m_ObjectStorage.end( ) ; pIter++ )
                        {
                                strget=pIter->first;

                                if(strname==strget)
                                {
                                        pbase=pIter->second;
                                        delete pbase;
                                        m_ObjectStorage.erase(pIter);
                                        return;
                                }
                        }
                }

                virtual void* addpointvar(const string_type & strname,void *pclassobject)
                {
                        (void)strname;
                        (void)pclassobject;
                        return 0;
                }
                virtual bool findvar(const string_type & a_strVarName)
                {
                         typename OrgStorage_type::iterator item = m_ObjectStorage.find(a_strVarName);
                        if (item!=m_ObjectStorage.end())
                                return true;
                        else
                                return false;
                };
                virtual const char*  getclass() const
                {
                        return  typeid(TCLASS).name();
                };
                virtual void * getvar(const string_type & a_str)
                {
                         typename OrgStorage_type::iterator item = m_ObjectStorage.find(a_str);
                        if (item!=m_ObjectStorage.end())
                                return item->second;
                        else
                                return 0;
                };
                virtual const char* getvar(int a_num)
                {
                        typename OrgStorage_type::const_iterator item = m_ObjectStorage.begin();
                        int inum=0;
                        for (; item!=m_ObjectStorage.end(); ++item)
                        {
                                if(a_num==inum)
                                        return (item->first).c_str();
                                ++inum;
                        }
                        return string_type("none").c_str();
                }
                virtual void * getvarpoint(int a_num)
                {
                        typename OrgStorage_type::const_iterator item = m_ObjectStorage.begin();
                        int inum=0;
                        for (; item!=m_ObjectStorage.end(); ++item)
                        {
                                if(a_num==inum)
                                        return item->second;
                                ++inum;
                        }
                        return 0;
                }
                virtual void * getvarpoint(const string_type & a_str)
                {
                        typename OrgStorage_type::iterator item = m_ObjectStorage.find(a_str);
                        if (item!=m_ObjectStorage.end())
                                return item->second;
                        else
                                return 0;
                };
                virtual const char* getpointvar(int a_num)
                {
                        (void)a_num;
                        return string_type("none").c_str();
                }
                virtual void Setpointvarp(const string_type & a_str,void *pvar)
                {
                        (void)a_str;
                        (void)pvar;
                }

                virtual void * getpointvarp(int a_num)
                {
                        (void)a_num;
                        return 0;
                }
                virtual int size()
                {
                        return static_cast<int>(m_ObjectStorage.size());
                }
                virtual int pointsize()
                {
                        return  0;
                }

        };

        template<class TCLASS>
        class ParserClass :public classbase
        {

                TCLASS *m_p;

                typedef std::map<string_type,TCLASS*> ClassStorage_type;
                typedef std::vector<TCLASS*> ParsVect;

                ClassStorage_type m_ObjectStorage;
                ParsVect m_Stack;

                ClassStorage_type m_PointObjectStorage;
                typedef std::map<string_type, classbasefunc*> classfunmap_type;
                classfunmap_type m_FunCmap;
                template<typename TMETHOD>
                void StoreRuntimeClassFun(const string_type &strname, TMETHOD method)
                {
                        classfunmap_type::iterator item = m_FunCmap.find(strname);
                        if (item!=m_FunCmap.end())
                                return;
                        classfuncstorage<TCLASS> *apclassfunc = new classfuncstorage<TCLASS>;
                        apclassfunc->StorageFUNC(method);
                        m_FunCmap[strname] = apclassfunc;
                }

        public:

                ParserClass()
                {
                        m_iclasstype = CLASS_PARSER;
                }
                virtual ~ParserClass()
                {

                        classfunmap_type:: iterator pFcIter;
                        for ( pFcIter = m_FunCmap.begin( ) ; pFcIter != m_FunCmap.end( ) ; pFcIter++ )
                        {
                                classbasefunc *pbasefun=pFcIter->second;
                                delete pbasefun;
                                pFcIter->second=0;
                        }

                        typename ClassStorage_type:: iterator pIter;
                        for ( pIter = m_ObjectStorage.begin( ) ; pIter != m_ObjectStorage.end( ) ; pIter++ )
                        {
                                TCLASS *pbase=pIter->second;
                                delete pbase;
                                pIter->second=0;
                        }

                };
                void deleteself(){delete this;};

                virtual void* addvar(const string_type & strname)
                {
                        typename ClassStorage_type::iterator item = m_ObjectStorage.find(strname);
                        if (item!=m_ObjectStorage.end())
                                return 0;
                        TCLASS *apclass=new TCLASS;
                        m_ObjectStorage[strname]=apclass;
                        m_Stack.push_back(apclass);
                        return apclass;
                };
                virtual int findstacknum(void *pclassobj)
                {
                        typename ParsVect::iterator pIter;
                        int inum = 0;
                        for(pIter = m_Stack.begin();pIter != m_Stack.end() ; pIter++)
                        {
                                TCLASS *pbase = *pIter;
                                if(pbase == pclassobj)
                                        return inum;
                                inum ++;
                        }
                        return -1;
                };
                virtual void clearvar()
                {
                        typename ClassStorage_type:: iterator pIter;
                        for ( pIter = m_ObjectStorage.begin( ) ; pIter != m_ObjectStorage.end( ) ; pIter++ )
                        {
                                TCLASS *pbase=pIter->second;
                                delete pbase;
                                pIter->second=0;
                        }

                        m_ObjectStorage.clear();
                }
                virtual void delvar(void *pobj)
                {
                        typename ClassStorage_type:: iterator pIter;
                        TCLASS *pbase;
                        for ( pIter = m_ObjectStorage.begin( ) ; pIter != m_ObjectStorage.end( ) ; pIter++ )
                        {
                                pbase=pIter->second;
                                if(pobj==pbase)
                                {
                                        delete pbase;
                                        m_ObjectStorage.erase(pIter);
                                        return;
                                }
                        }
                }
                virtual void delvar(const string_type& strname)
                {
                        typename ClassStorage_type:: iterator pIter;
                        TCLASS *pbase;
                        string_type strget;
                        for ( pIter = m_ObjectStorage.begin( ) ; pIter != m_ObjectStorage.end( ) ; pIter++ )
                        {
                                strget=pIter->first;

                                if(strname==strget)
                                {
                                        pbase=pIter->second;
                                        delete pbase;
                                        m_ObjectStorage.erase(pIter);
                                        return;
                                }
                        }
                }

                virtual void* addpointvar(const string_type & strname,void *pclassobject)
                {
                        typename ClassStorage_type::iterator item = m_PointObjectStorage.find(strname);
                        if (item!=m_PointObjectStorage.end())
                                return 0;
                        TCLASS *apclass=(TCLASS *)pclassobject;
                        m_PointObjectStorage[strname]=apclass;
                        return pclassobject;
                }
                virtual bool findvar(const string_type & a_strVarName)
                {
                        typename ClassStorage_type::iterator item = m_ObjectStorage.find(a_strVarName);
                        if (item!=m_ObjectStorage.end())
                                return true;
                        item = m_PointObjectStorage.find(a_strVarName);
                        if (item!=m_PointObjectStorage.end())
                                return true;
                        return false;
                };
                virtual const char*   getclass() const
                {
                        return  typeid(TCLASS).name();
                };

                virtual void * getvar(const string_type & a_str)
                {
                        typename ClassStorage_type::iterator item = m_ObjectStorage.find(a_str);
                        if (item!=m_ObjectStorage.end())
                                return  item->second;
                        item = m_PointObjectStorage.find(a_str);
                        if (item!=m_PointObjectStorage.end())
                                return item->second;

                        return 0;
                };

                virtual const char* getvar(int a_num)
                {
                        typename ClassStorage_type::const_iterator item = m_ObjectStorage.begin();
                        int inum=0;
                        for (; item!=m_ObjectStorage.end(); ++item)
                        {
                                if(a_num==inum)
                                        return (item->first).c_str();
                                ++inum;
                        }
                        return string_type("none").c_str();
                }
                virtual void * getvarpoint(int a_num)
                {
                        typename ClassStorage_type::const_iterator item = m_ObjectStorage.begin();
                        int inum=0;
                        for (; item!=m_ObjectStorage.end(); ++item)
                        {
                                if(a_num==inum)
                                        return item->second;
                                ++inum;
                        }
                        return 0;
                }
                virtual void * getvarpoint(const string_type & a_str)
                {
                        typename ClassStorage_type::iterator item = m_ObjectStorage.find(a_str);
                        if (item!=m_ObjectStorage.end())
                                return  item->second;
                        item = m_PointObjectStorage.find(a_str);
                        if (item!=m_PointObjectStorage.end())
                                return item->second;

                        return 0;
                };
                virtual const char* getpointvar(int a_num)
                {
                        typename ClassStorage_type::const_iterator item = m_PointObjectStorage.begin();
                        int inum=0;
                        for (; item!=m_PointObjectStorage.end(); ++item)
                        {
                                if(a_num==inum)
                                        return (item->first).c_str();
                                ++inum;
                        }
                        return string_type("none").c_str();

                }
                virtual void * getpointvarp(int a_num)
                {
                        typename ClassStorage_type::const_iterator item = m_PointObjectStorage.begin();
                        int inum=0;
                        for (; item!=m_PointObjectStorage.end(); ++item)
                        {
                                if(a_num==inum)
                                        return item->second;
                                ++inum;
                        }
                        return 0;
                }
                virtual void Setpointvarp(const string_type & a_str,void *pvar)
                {
                        (void)a_str;
                        typename ClassStorage_type::iterator item = m_ObjectStorage.find(a_str);
                        item = m_PointObjectStorage.find(a_str);
                        if (item!=m_PointObjectStorage.end())
                        {
                                item->second=(TCLASS *)pvar;
                        }
                }
                virtual int size()
                {
                        return static_cast<int>(m_ObjectStorage.size());
                }
                virtual int pointsize()
                {
                        return static_cast<int>(m_PointObjectStorage.size());
                }

                void AddClassFun(const string_type & strname,void (TCLASS::*pdf)(void ))
                {
                        
                        StoreRuntimeClassFun(strname, pdf);
                }
                void AddClassFun(const string_type & strname,void (TCLASS::*pdf1)(double ))
                {
                        StoreRuntimeClassFun(strname, pdf1);
                }
                void AddClassFun(const string_type & strname,void (TCLASS::*pdf2)(double,double ))
                {
                        StoreRuntimeClassFun(strname, pdf2);
                }
                void AddClassFun(const string_type & strname,void (TCLASS::*pdf3)(double,double,double))
                {
                        StoreRuntimeClassFun(strname, pdf3);
                }
                void AddClassFun(const string_type & strname,void (TCLASS::*pdf4)(double,double,double,double))
                {
                        StoreRuntimeClassFun(strname, pdf4);
                }
                void AddClassFun(const string_type & strname,void (TCLASS::*pif1)(int))
                {
                        StoreRuntimeClassFun(strname, pif1);
                }
                void AddClassFun(const string_type & strname,void (TCLASS::*pif2)(int,int))
                {
                        StoreRuntimeClassFun(strname, pif2);
                }
                void AddClassFun(const string_type & strname,void (TCLASS::*pifd2)(int,double))
                {
                        StoreRuntimeClassFun(strname, pifd2);
                }
                void AddClassFun(const string_type & strname,void (TCLASS::*pdfi2)(double,int))
                {
                        StoreRuntimeClassFun(strname, pdfi2);
                }
                void AddClassFun(const string_type & strname,void (TCLASS::*pif3)(int,int,int))
                {
                        StoreRuntimeClassFun(strname, pif3);
                }
                void AddClassFun(const string_type & strname,void (TCLASS::*pif4)(int,int,int,int))
                {
                        StoreRuntimeClassFun(strname, pif4);
                }
                void AddClassFun(const string_type & strname,void (TCLASS::*pif5)(int,int,int,int,int))
                {
                        StoreRuntimeClassFun(strname, pif5);
                }
                void AddClassFun(const string_type & strname,void (TCLASS::*pif6)(int,int,int,int,int,int))
                {
                        StoreRuntimeClassFun(strname, pif6);
                }
                void AddClassFun(const string_type & strname,void (TCLASS::*pif7)(int,int,int,int,int,int,int))
                {
                        StoreRuntimeClassFun(strname, pif7);
                }

                void AddClassFun(const string_type & strname,double (TCLASS::*pdfrd)(void* ))
                {
                        StoreRuntimeClassFun(strname, pdfrd);
                }

                void AddClassFun(const string_type & strname,void (TCLASS::*pdf)(void* ))
                {
                        StoreRuntimeClassFun(strname, pdf);
                }

                void AddClassFun(const string_type & strname,void (TCLASS::*pcf1)(const char_type *))
                {
                        StoreRuntimeClassFun(strname, pcf1);
                }
                void AddClassFun(const string_type & strname,void (TCLASS::*pdfc2)(double,const char_type *))
                {
                        StoreRuntimeClassFun(strname, pdfc2);
                }
                void AddClassFun(const string_type & strname,void (TCLASS::*pfc_any)(charpvect &))
                {
                        StoreRuntimeClassFun(strname, pfc_any);
                }
                void AddClassFun(const string_type & strname,void (TCLASS::*pany)(paramvect &))
                {
                        StoreRuntimeClassFun(strname, pany);
                }
                void AddClassFun(const string_type & strname,char_type * (TCLASS::*pf0_re_c)(void))
                {
                        StoreRuntimeClassFun(strname, pf0_re_c);
                }
                void AddClassFun(const string_type & strname,double (TCLASS::*pif1_re_db)(int))
                {
                        StoreRuntimeClassFun(strname, pif1_re_db);
                }
                void AddClassFun(const string_type & strname,double (TCLASS::*pif2_re_db)(int,int))
                {
                        StoreRuntimeClassFun(strname, pif2_re_db);
                }
                void AddClassFun(const string_type & strname,double (TCLASS::*pifd2_re_db)(int,double))
                {
                        StoreRuntimeClassFun(strname, pifd2_re_db);
                }
                void AddClassFun(const string_type & strname,double (TCLASS::*pdfi2_re_db)(double,int))
                {
                        StoreRuntimeClassFun(strname, pdfi2_re_db);
                }

                void AddClassFun(const string_type & strname,double (TCLASS::*pif3_re_db)(int,int,int))
                {
                        StoreRuntimeClassFun(strname, pif3_re_db);
                }

                void AddClassFun(const string_type & strname,int (TCLASS::*pif2_re_int)(int,int))
                {
                        StoreRuntimeClassFun(strname, pif2_re_int);
                }
                void AddClassFun(const string_type & strname,int (TCLASS::*pifd2_re_int)(int,double))
                {
                        StoreRuntimeClassFun(strname, pifd2_re_int);
                }
                void AddClassFun(const string_type & strname,int (TCLASS::*pdfi2_re_int)(double,int))
                {
                        StoreRuntimeClassFun(strname, pdfi2_re_int);
                }
                void AddClassFun(const string_type & strname,int (TCLASS::*pif1_re_int)(int))
                {
                        StoreRuntimeClassFun(strname, pif1_re_int);
                }
                void AddClassFun(const string_type & strname,int (TCLASS::*pf0_re_int)( ))
                {
                        StoreRuntimeClassFun(strname, pf0_re_int);
                }
                void AddClassFun(const string_type & strname,double (TCLASS::*pf0_re_double)( ))
                {
                        StoreRuntimeClassFun(strname, pf0_re_double);
                }
                void AddClassFun(const string_type & strname,int (TCLASS::*pany_re_int)(paramvect &))
                {
                        StoreRuntimeClassFun(strname, pany_re_int);
                }
                void AddClassFun(const string_type & strname,int (TCLASS::*pfc_any_re_int)(charpvect &))
                {
                        StoreRuntimeClassFun(strname, pfc_any_re_int);
                }
                void AddClassFun(const string_type & strname,double (TCLASS::*pany_re_double)(paramvect &))
                {
                        StoreRuntimeClassFun(strname, pany_re_double);
                }
                void AddClassFun(const string_type & strname,double (TCLASS::*pdfc2_re_double)(double,const char_type *))
                {
                        StoreRuntimeClassFun(strname, pdfc2_re_double);
                }
                void AddClassFun(const string_type & strname,double (TCLASS::*pfc_any_re_double)(charpvect &))
                {
                        StoreRuntimeClassFun(strname, pfc_any_re_double);
                }

                virtual bool findclassfun(const string_type &a_strName)
                {
                        classfunmap_type::iterator item = m_FunCmap.find(a_strName);
                        if (item==m_FunCmap.end())
                                return  false;
                        else
                                return true;
                }
                virtual int GetFuncArgCount(const string_type &a_strName)
                {
                        classfunmap_type::iterator item = m_FunCmap.find(a_strName);
                        if (item==m_FunCmap.end())
                                return  0;
                        classbasefunc  *apclassfunc= item->second;
                        return apclassfunc->GetArgCount();
                }
                virtual int GetFuncArgType(const string_type &a_strName)
                {
                        classfunmap_type::iterator item = m_FunCmap.find(a_strName);
                        if (item==m_FunCmap.end())
                                return  0;
                        classbasefunc  *apclassfunc= item->second;
                        return apclassfunc->GetArgType();
                }
                virtual ClassFunSignatureShape GetFuncSignatureShape(const string_type &a_strName)
                {
                        return DescribeClassFunSignature(GetFuncArgType(a_strName));
                }

                virtual double ApplyClassFunc(void *pobj,const string_type &strname,paramvect& parm)
                {
                        classfunmap_type::iterator item = m_FunCmap.find(strname);
                        if (item==m_FunCmap.end())
                                return  0;
                        classbasefunc  *apclassfunc= item->second;
                        apclassfunc->SetClassObj(pobj);
                        apclassfunc->AddParam(parm);
                        return apclassfunc->UseFUNC_Return();
                };

                virtual double ApplyClassFunc(void *pobj,void  *apclassfunc,paramvect& parm)
                {
                        classbasefunc *pfunc =(classbasefunc *) apclassfunc;
                        pfunc->SetClassObj(pobj);
                        pfunc->AddParam(parm);
                        return pfunc->UseFUNC_Return();
                };
                virtual double ApplyClassFunc(void *pobj,const string_type &strname,voidparamvect& parm)
                {
                        classfunmap_type::iterator item = m_FunCmap.find(strname);
                        if (item==m_FunCmap.end())
                                return  0;
                        classbasefunc  *apclassfunc= item->second;
                        apclassfunc->SetClassObj(pobj);
                        apclassfunc->AddVoidpParam(parm);

                                        return apclassfunc->UseFUNC_Return();

                };

                virtual double ApplyClassFunc(void *pobj,void * pclassfuc,voidparamvect& parm)
                {
                        classbasefunc  *apclassfunc = (classbasefunc  *)pclassfuc;
                        apclassfunc->SetClassObj(pobj);
                        apclassfunc->AddVoidpParam(parm);

                        return apclassfunc->UseFUNC_Return();
                };

                virtual double ApplyClassFunc(void *pobj,const string_type &strname,charpvect& parm)
                {
                        classfunmap_type::iterator item = m_FunCmap.find(strname);
                        if (item==m_FunCmap.end())
                                return  0;
                        classbasefunc  *apclassfunc= item->second;
                        apclassfunc->SetClassObj(pobj);
                        apclassfunc->AddCharpParam(parm);

                        return apclassfunc->UseFUNC_Return();

                };
                virtual double ApplyClassFunc(void *pobj,const string_type &strname,paramvect& parm,charpvect& charparm)
                {
                        classfunmap_type::iterator item = m_FunCmap.find(strname);
                        if (item==m_FunCmap.end())
                                return  0;
                        classbasefunc  *apclassfunc= item->second;
                        apclassfunc->SetClassObj(pobj);
                        apclassfunc->AddParam(parm);
                        apclassfunc->AddCharpParam(charparm);
                        return apclassfunc->UseFUNC_Return();
                };

                virtual double ApplyClassFunc(void *pobj,void * pclassfun,charpvect& parm)
                {
                        classbasefunc  *apclassfunc =(classbasefunc*) pclassfun;
                        apclassfunc->SetClassObj(pobj);
                        apclassfunc->AddCharpParam(parm);
                        return apclassfunc->UseFUNC_Return();
                };
                virtual double ApplyClassFunc(void *pobj,void * pclassfun,paramvect& parm,charpvect& charparm)
                {
                        classbasefunc  *apclassfunc =(classbasefunc*) pclassfun;
                        apclassfunc->SetClassObj(pobj);
                        apclassfunc->AddParam(parm);
                        apclassfunc->AddCharpParam(charparm);
                        return apclassfunc->UseFUNC_Return();
                };
                virtual char* ApplyClassFuncString(void *pobj,const string_type &strname)
                {
                        classfunmap_type::iterator item = m_FunCmap.find(strname);
                        if (item==m_FunCmap.end())
                                return  0;
                        classbasefunc  *apclassfunc= item->second;
                        apclassfunc->SetClassObj(pobj);
                        return apclassfunc->UseFUNC_ReturnChar();
                };

                virtual void* GetClassFuncLP(const string_type &strname)
                {
                        classfunmap_type::iterator item = m_FunCmap.find(strname);
                        if (item==m_FunCmap.end())
                                return  0;
                        classbasefunc  *apclassfunc= item->second;
                        return (void*)apclassfunc;
                }
                virtual const char* getfuncname(int a_num)
                {
                        classfunmap_type::iterator item = m_FunCmap.begin();
                        int inum=0;
                        for (; item!=m_FunCmap.end(); ++item)
                                {
                                if(a_num==inum)
                                        return (item->first).c_str();
                                ++inum;
                                }
                        return string_type("none").c_str();
                }
                virtual const char* getfunctype(int a_num)
                {
                        classfunmap_type::iterator item = m_FunCmap.begin();
                        int inum=0;
                        for (; item!=m_FunCmap.end(); ++item)
                        {
                                if(a_num==inum)
                                {
                                        classbasefunc *pfunc =item->second;
                                        switch(pfunc->GetArgType())
                                        {
                                                case Param_none:
                                                        return "Param_none";
                                                case Param_voidp_1:
                                                        return "Param_voidp_1";

                                                case Param_voidp_1_Return_double:
                                                        return "Param_voidp_1_Return_double";
                                                case Param_double_1:
                                                        return "Param_double_1";
                                                case Param_double_2:
                                                        return "Param_double_2";
                                                case Param_double_3:
                                                        return "Param_double_3" ;
                                                case Param_double_4:
                                                        return "Param_double_4" ;
                                                case Param_int_1:
                                                        return "Param_int_1" ;
                                                case Param_int_2:
                                                        return "Param_int_2" ;
                                                case Param_int_3:
                                                        return "Param_int_3" ;
                                                case Param_int_4:
                                                        return "Param_int_4" ;
                                                case Param_int_5:
                                                        return "Param_int_5" ;
                                                case Param_int_6:
                                                        return "Param_int_6" ;
                                                case Param_int_7:
                                                        return "Param_int_7" ;
                                                case Param_0_Return_int:
                                                        return "Param_0_Return_int";
                                                case Param_0_Return_double:
                                                        return "Param_0_Return_double";
                                                case Param_int_1_Return_int:
                                                        return "Param_int_1_Return_int";
                                                case Param_int_2_Return_int:
                                                        return "Param_int_2_Return_int";
                                                case Param_int_1_Return_double:
                                                        return "Param_int_1_Return_double";
                                                case Param_int_2_Return_double:
                                                        return "Param_int_2_Return_double";
                                                case Param_int_3_Return_double:
                                                        return "Param_int_3_Return_double";
                                                case Param_charp_1:
                                                        return "Param_charp_1";
                                                case Param_double_charp_2:
                                                        return "Param_double_charp_2";
                                                case Param_double_charp_2_Return_double:
                                                        return "Param_double_charp_2_Return_double";
                                                case Param_charp_any:
                                                        return "Param_charp_any";
                                                case Param_0_Return_charp:
                                                        return "Param_0_Return_charp";
                                                case Param_any:
                                                        return "Param_any";
                                                case Param_any_Return_int:
                                                        return "Param_any_Return_int";
                                                case Param_any_Return_double:
                                                        return "Param_any_Return_double";
                                                case Param_charp_any_Return_int:
                                                        return "Param_charp_any_Return_int";
                                                case Param_charp_any_Return_double:
                                                        return "Param_charp_any_Return_double";

                                                default:
                                                        return "Param_any" ;
                                        }

                                }

                                ++inum;
                        }
                        return string_type("none").c_str();

                }
                virtual int funcsize()
                {
                        return static_cast<int>(m_FunCmap.size());
                }

                virtual classbase*GetMe()
                {
                        return this;
                }

        private:
                void *m_pFun;
        };

        class CreateClass :public classbase
        {
        private:

                typedef std::map<string_type,string_type> strmapfunc_type;

                typedef struct stringvirclass
                {
                        string_type m_classdef;
                        strmapfunc_type m_classfuncdef;
                }strclass;

                typedef std::vector<string_type> stringmember_type;
                typedef std::vector<classbase *> classdefbuf_type;
                typedef std::vector<void *> classobjbuf_type;

                typedef std::vector<string_type> stringbuf_type;
                typedef std::vector<int> storage_type;

                typedef struct 	fuctioncodestruct
                {
                        storage_type m_bytecode;
                        string_type m_strbuf;

                }fucstruct;

                typedef struct classobjectstruct
                {
                        stringmember_type m_objname;
                        classobjbuf_type m_objbuf;
                        bool m_ctor_ran;
                }objstruct;

                typedef std::map<string_type,fucstruct*> funcodemap_type;
                typedef std::map<string_type,objstruct*> objmap_type;

        public:

                string_type m_classname;

                strclass  m_ClassStrmap;

                funcodemap_type m_codemap;

                stringmember_type m_classStr;
        classdefbuf_type m_classdefbuf;

                objmap_type m_objmap;
        public:
                ParserBase *m_pCurParser;
                CreateClass()
                {
                        m_iclasstype = CLASS_PARSER;
                };
                CreateClass(const string_type &a_strName,ParserBase *pParser)
                {
                        m_pCurParser = pParser;
                        m_classname = a_strName;
                };

                void deleteself(){delete this;};
                virtual const char*   getclass() const
                {
                        return  m_classname.c_str();
                };
                virtual ~CreateClass()
                {
                        releaseclassfun();
                        clearvar();
                };
                void releaseclassfun()
                {
                        funcodemap_type:: iterator pIter;
                        fucstruct *pfucstruct;
                        for ( pIter = m_codemap.begin( ) ; pIter != m_codemap.end( ) ; pIter++ )
                        {
                                pfucstruct = pIter->second;
                                delete pfucstruct;
                                pIter->second=0;
                        }
                }

                void GetRunParser(ParserBase* pParser)
                {
                        m_pCurParser = pParser;
                }

                bool addclassdef(classbase*pclassdef,const string_type & strname)
                {

                        if(NULL == pclassdef)
                                return false;
                        stringmember_type::iterator pIter;
                        int inum = 0;
                        for(pIter = m_classStr.begin();pIter != m_classStr.end() ; pIter++)
                        {
                                if(*pIter  == strname)
                                        return false;
                                inum ++;
                        }
                        m_classStr.push_back(strname);
                        m_classdefbuf.push_back(pclassdef);
                        return true;
                }
                int FindMemberSlot(const string_type &member_name) const
                {
                        for (int i = 0; i < static_cast<int>(m_classStr.size()); ++i)
                        {
                                if (m_classStr[i] == member_name)
                                        return i;
                        }
                        return -1;
                }
                classbase *GetMemberClass(const string_type &member_name) const
                {
                        const int slot = FindMemberSlot(member_name);
                        if (slot < 0)
                                return NULL;
                        return m_classdefbuf[slot];
                }
                string_type BuildMemberStorageName(const string_type &object_name,
                                                   const string_type &member_name) const
                {
                        return m_classname + _T("@") + object_name + _T("@") + member_name;
                }
                bool FindObjectBindingName(void *pobj, string_type &object_name) const;
                const char *FindCtorFuncDef() const
                {
                        const char *ctor_func = GetFuncDef("__ctor__");
                        if (ctor_func != NULL)
                                return ctor_func;

                        const char *factory_func = GetFuncDef("__factory__");
                        if (factory_func != NULL)
                                return factory_func;

                        return GetFuncDef("create");
                }

                bool AddClassFun(const string_type &a_strName, const string_type &a_strFun)
                {
                        
                        funcodemap_type:: iterator pIter;
                        fucstruct *pfucstruct;
                        for ( pIter = m_codemap.begin( ) ; pIter != m_codemap.end( ) ; pIter++ )
                        {
                                pfucstruct = pIter->second;
                                if(pfucstruct->m_strbuf == a_strName)
                                        return false;
                        }
                        pfucstruct = new fucstruct;
                        pfucstruct->m_strbuf = a_strFun;
                        m_codemap[a_strName] = pfucstruct;
                        return true;

                }
                virtual void * addvar(const string_type & strobjname);
                virtual void clearvar()
                {
                        objmap_type:: iterator pIter;
                        for ( pIter = m_objmap.begin( ) ; pIter != m_objmap.end( ) ; pIter++ )
                        {
                                objstruct *pstruct=pIter->second;
                                delete pstruct;
                                pIter->second=0;
                        }
                        m_objmap.clear();
                }
                virtual int funcsize()
                {
                        return static_cast<int>(m_codemap.size());
                }

                virtual int size()
                {
                        return static_cast<int>(m_objmap.size());
                }
                virtual bool findvar(const string_type & a_strVarName)
                {
                        objmap_type::iterator item = m_objmap.find(a_strVarName);
                        if (item!=m_objmap.end())
                                return true;
                        return false;
                };
                virtual void * getvar(const string_type & a_str)
                {
                        objmap_type::iterator item = m_objmap.find(a_str);
                        if (item!=m_objmap.end())
                                return item->second;

                        return 0;
                };
                virtual const char* getvar(int a_num)
                {
                        objmap_type::iterator item = m_objmap.begin();
                        int inum=0;
                        for (; item!=m_objmap.end(); ++item)
                        {
                                if(a_num==inum)
                                        return (item->first).c_str();
                                ++inum;
                        }
                        return string_type("none").c_str();
                }
                virtual void * getvarpoint(int a_num)
                {
                        objmap_type::iterator item = m_objmap.begin();
                        int inum=0;
                        for (; item!=m_objmap.end(); ++item)
                        {
                                if(a_num==inum)
                                        return item->second;
                                ++inum;
                        }
                        return 0;
                }
                virtual void * getvarpoint(const string_type & a_str)
                {
                        objmap_type::iterator item = m_objmap.find(a_str);
                        if (item!=m_objmap.end())
                                return item->second;

                        return 0;
                };
                virtual const char* getpointvar(int a_num)
                {
                        (void)a_num;
                        return "none";
                }
                virtual void * getpointvarp(int a_num)
                {
                        (void)a_num;

                        return 0;
                }
                virtual bool findclassfun(const string_type &a_strName)
                {
                        funcodemap_type::iterator item = m_codemap.find(a_strName);
                        if (item==m_codemap.end())
                                return  false;
                        else
                                return true;
                }
                virtual const char* getfuncname(int a_num)
                {
                        funcodemap_type:: iterator item = m_codemap.begin();
                        int inum=0;
                        for (; item!=m_codemap.end(); ++item)
                        {
                                if(a_num==inum)
                                        return (item->first).c_str();
                                ++inum;
                        }
                        return string_type("none").c_str();
                }
                virtual const char* getfunctype(int a_num)
                {
                        funcodemap_type:: iterator item = m_codemap.begin();
                        int inum=0;
                        for (; item!=m_codemap.end(); ++item)
                        {
                                if(a_num==inum)
                                {
                                        return "no define type";

                                }

                                ++inum;
                        }
                        return "none";

                }
                virtual int GetFuncArgCount(const string_type &a_strName)
                {
                        if (a_strName == "__ctor__" ||
                            a_strName == "__factory__" ||
                            a_strName == "create")
                                return -1;

                        return 0;
                }
                virtual int GetFuncArgType(const string_type &a_strName)
                {
                        if (a_strName == "__ctor__" ||
                            a_strName == "__factory__" ||
                            a_strName == "create")
                                return Param_any;
                        return Param_none;
                }
                virtual ClassFunSignatureShape GetFuncSignatureShape(const string_type &a_strName)
                {
                        return DescribeClassFunSignature(GetFuncArgType(a_strName));
                }
                virtual double ApplyClassFunc(void *pobj,const string_type &a_strName,paramvect& parm);
                virtual double ApplyClassFunc(void *pobj,void  *apclassfunc,paramvect& parm);

                virtual double ApplyClassFunc(void *pobj,void  *apclassfunc,voidparamvect& parm);

                virtual double ApplyClassFunc(void *pobj,void  *apclassfunc,charpvect& parm);
                const char * GetClassMemberName(int inum) const
                {
                        return m_classStr[inum].c_str();
                }
                const char * GetClassDefName(int inum) const
                {
                        return m_classdefbuf[inum]->getclass();
                }
                int GetClassMemberNum() const
                {
                        return static_cast<int>(m_classStr.size());
                }
                const char * GetFuncDef(const char *pFuncname) const
                {
                        string_type astr(pFuncname);
                        funcodemap_type::const_iterator item = m_codemap.find(astr);
                        if (item!=m_codemap.end())
                                return item->second->m_strbuf.c_str();
                        return 0;
                }
                virtual bool Iscreateclass()
                {
                        return true;
                }
        };

        typedef std::map<string_type, mu::classbase*> classbasemap_type;

}

#endif
