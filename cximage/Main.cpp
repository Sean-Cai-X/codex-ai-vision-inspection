
#include "ViewController.h"
#include "Main.h"
#include <cstdlib>
#include <exception>
#include <iostream>
int glfw_occ_main ( )
{
  std::cerr << "[cxvision_imgui_acceptance] build=20260901_opengl_guard_v2"
            << std::endl;
  ViewController anApp;
  try
  {
    anApp.run();
  }
  catch (const std::exception& theError)
  {
    std::cerr << "[GUI_INIT_FAILED] " << theError.what() << std::endl;
    return EXIT_FAILURE;
  }
  catch (...)
  {
    std::cerr << "[GUI_INIT_FAILED] unknown exception" << std::endl;
    return EXIT_FAILURE;
  }
  return 0;
}
