
#pragma once
#include <string>
#include <vector>
struct CxGrayBoundaryUiState {
 std::string output_path,receipt,summary="NOT RUN";
 std::string executed_script,executed_case,executed_image,executed_sha,view_sha;
 std::string pending_script,pending_case,pending_image,pending_sha;
 bool valid=false,pending=false;
 std::vector<double> raw,rejected;
 std::vector<std::vector<double>> refined;
};
