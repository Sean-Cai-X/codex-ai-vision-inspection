#include "DevAnalysisGuiApp.h"

#include <exception>
#include <iostream>
#include <string>

namespace
{
DevAnalysisGuiLaunchOptions ParseLaunchOptions(int argc, char** argv)
{
  DevAnalysisGuiLaunchOptions options;
  for (int index = 1; index < argc; ++index)
  {
    const std::string argument = argv[index] != nullptr ? argv[index] : "";
    auto consume_value = [&](std::string& target) {
      if ((index + 1) < argc && argv[index + 1] != nullptr)
      {
        target = argv[++index];
      }
    };

    if (argument == "--chain")
    {
      consume_value(options.chain_id);
    }
    else if (argument == "--case")
    {
      consume_value(options.case_name);
    }
    else if (argument == "--load-result")
    {
      consume_value(options.load_result_path);
    }
    else if (argument == "--capture-dir")
    {
      consume_value(options.capture_dir);
    }
  }
  return options;
}
}

int main(int argc, char** argv)
{
  try
  {
    DevAnalysisGuiApp app(ParseLaunchOptions(argc, argv));
    return app.Run();
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}