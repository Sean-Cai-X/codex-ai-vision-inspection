#ifndef CXIMAGE_CX_ALGORITHM_BUDGET_H
#define CXIMAGE_CX_ALGORITHM_BUDGET_H

struct CxAlgorithmBudget
{
    int max_elapsed_ms = 5000;
    int max_scan_lines = 4096;
    int max_samples = 200000;
};

struct CxAlgorithmBudgetState
{
    int elapsed_ms = 0;
    int scan_line_count = 0;
    int sample_count = 0;

    bool exceeded = false;
    std::string exceeded_kind;
};

#endif