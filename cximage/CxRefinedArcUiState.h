#pragma once
#include <string>
struct CxRefinedArcUiState {
 std::string receipt,summary="NOT RUN",output_path,executed_script,executed_case,pending_script,pending_case;
 std::string sha[2],pending_sha[2],paths[2],stamps[2];
 bool valid=false,pending=false;
};
