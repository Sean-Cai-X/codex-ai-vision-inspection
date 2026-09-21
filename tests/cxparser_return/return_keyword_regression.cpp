#include "muParser.h"
#include <iostream>
#include <string>

int main()
{
    int iStat = 0;
    mu::Parser p;
    double x = 0;
    double returnValue = 0;

    try
    {
        p.DefineVar("x", &x);
        p.DefineVar("returnValue", &returnValue);

        std::cout << "Test 1: x = 0; return; x = 1;" << std::endl;
        x = 0;
        p.SetExpr("x=0; return; x=1;");
        p.Eval();
        if (x != 0) { std::cout << "  FAILED: x=" << x << ", expected 0" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 2: x = 0; if (1) { return; } x = 1;" << std::endl;
        x = 0;
        p.SetExpr("x=0; if (1) { return; } x=1;");
        p.Eval();
        if (x != 0) { std::cout << "  FAILED: x=" << x << ", expected 0" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 3: x = 0; if (0) { return; } x = 1;" << std::endl;
        x = 0;
        p.SetExpr("x=0; if (0) { return; } x=1;");
        p.Eval();
        if (x != 1) { std::cout << "  FAILED: x=" << x << ", expected 1" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 4: x = 0; while (x < 3) { x=x+1; return; } x=9;" << std::endl;
        x = 0;
        p.SetExpr("x=0; while (x < 3) { x=x+1; return; } x=9;");
        p.Eval();
        if (x != 1) { std::cout << "  FAILED: x=" << x << ", expected 1" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 5: return ;" << std::endl;
        x = 0;
        p.SetExpr("return ;");
        p.Eval();
        if (x != 0) { std::cout << "  FAILED: x=" << x << ", expected 0" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 6: if (0) { return; } x=1;" << std::endl;
        x = 0;
        p.SetExpr("if (0) { return; } x=1;");
        p.Eval();
        if (x != 1) { std::cout << "  FAILED: x=" << x << ", expected 1" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 7: returnValue = 2; x=3;" << std::endl;
        returnValue = 0;
        x = 0;
        p.SetExpr("returnValue = 2; x=3;");
        p.Eval();
        if (returnValue != 2 || x != 3)
        {
            std::cout << "  FAILED: returnValue=" << returnValue
                      << ", x=" << x
                      << ", expected returnValue=2, x=3" << std::endl;
            iStat++;
        }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 8: if (1) { if (1) { return; } } x=1;" << std::endl;
        x = 0;
        p.SetExpr("if (1) { if (1) { return; } } x=1;");
        p.Eval();
        if (x != 0) { std::cout << "  FAILED: x=" << x << ", expected 0" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 9: return 1; (should fail)" << std::endl;
        bool threw = false;
        try
        {
            p.SetExpr("return 1;");
            p.Eval();
        }
        catch (mu::Parser::exception_type&)
        {
            threw = true;
        }
        if (!threw) { std::cout << "  FAILED: expected parser error" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 10: Parser reuse - x=2;" << std::endl;
        p.SetExpr("x=2;");
        p.Eval();
        if (x != 2) { std::cout << "  FAILED: x=" << x << ", expected 2" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 11: return after contract-like assignment" << std::endl;
        x = 0;
        p.SetExpr("x=1; return; x=2;");
        p.Eval();
        if (x != 1) { std::cout << "  FAILED: x=" << x << ", expected 1" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 12: return in false branch does not trigger" << std::endl;
        x = 0;
        p.SetExpr("x=0; if (0) { return; x=99; } x=1;");
        p.Eval();
        if (x != 1) { std::cout << "  FAILED: x=" << x << ", expected 1" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 13: Parser reuse after return - run next script" << std::endl;
        x = 0;
        p.SetExpr("return;");
        p.Eval();
        p.SetExpr("x=5;");
        p.Eval();
        if (x != 5) { std::cout << "  FAILED: x=" << x << ", expected 5" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 14: returnValue is still a normal variable" << std::endl;
        returnValue = 0;
        p.SetExpr("returnValue = 2;");
        p.Eval();
        if (returnValue != 2) { std::cout << "  FAILED: returnValue=" << returnValue << ", expected 2" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 15: return_method is still a normal identifier" << std::endl;
        double return_method = 0;
        p.DefineVar("return_method", &return_method);
        p.SetExpr("return_method = 3;");
        p.Eval();
        if (return_method != 3) { std::cout << "  FAILED: return_method=" << return_method << ", expected 3" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 16: return_status is still a normal identifier" << std::endl;
        double return_status = 0;
        p.DefineVar("return_status", &return_status);
        p.SetExpr("return_status = 4;");
        p.Eval();
        if (return_status != 4) { std::cout << "  FAILED: return_status=" << return_status << ", expected 4" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 17: return as identifier prefix - returnValue" << std::endl;
        x = 0;
        returnValue = 0;
        p.SetExpr("returnValue = 5; x=1;");
        p.Eval();
        if (returnValue != 5 || x != 1) { std::cout << "  FAILED: returnValue=" << returnValue << ", x=" << x << ", expected returnValue=5, x=1" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 18: return at end of script - parse ok" << std::endl;
        x = 0;
        p.SetExpr("x=1; return;");
        p.Eval();
        if (x != 1) { std::cout << "  FAILED: x=" << x << ", expected 1" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 19: return x; (should fail)" << std::endl;
        threw = false;
        try
        {
            p.SetExpr("return x;");
            p.Eval();
        }
        catch (mu::Parser::exception_type&)
        {
            threw = true;
        }
        if (!threw) { std::cout << "  FAILED: expected parser error" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;

        std::cout << "Test 20: return object; (should fail)" << std::endl;
        threw = false;
        try
        {
            p.SetExpr("return obj;");
            p.Eval();
        }
        catch (mu::Parser::exception_type&)
        {
            threw = true;
        }
        if (!threw) { std::cout << "  FAILED: expected parser error" << std::endl; iStat++; }
        else std::cout << "  PASSED" << std::endl;
    }
    catch (mu::Parser::exception_type& e)
    {
        std::cout << "Unexpected parser exception: " << e.GetMsg() << std::endl;
        iStat++;
    }
    catch (std::exception& e)
    {
        std::cout << "Unexpected std exception: " << e.what() << std::endl;
        iStat++;
    }
    catch (...)
    {
        std::cout << "Unexpected unknown exception" << std::endl;
        iStat++;
    }

    std::cout << std::endl;
    if (iStat == 0)
    {
        std::cout << "ALL 20 TESTS PASSED" << std::endl;
        return 0;
    }
    else
    {
        std::cout << "TESTS FAILED with " << iStat << " errors" << std::endl;
        return 1;
    }
}