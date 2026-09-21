
#include "Window.h"

#if defined (__APPLE__)
  #undef Handle // avoid name collisions in macOS headers
  #define GLFW_EXPOSE_NATIVE_COCOA
  #define GLFW_EXPOSE_NATIVE_NSGL
#elif defined (_WIN32)
  #define GLFW_EXPOSE_NATIVE_WIN32
  #define GLFW_EXPOSE_NATIVE_WGL
#else
  #define GLFW_EXPOSE_NATIVE_X11
  #define GLFW_EXPOSE_NATIVE_GLX
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

// ================================================================
// Function : Window
// Purpose  :
// ================================================================
Window::Window (int theWidth, int theHeight, const TCollection_AsciiString& theTitle)
: myGlfwWindow (glfwCreateWindow (theWidth, theHeight, theTitle.ToCString(), NULL, NULL)),
  myXLeft  (0),
  myYTop   (0),
  myXRight (0),
  myYBottom(0)
{
  if (myGlfwWindow != nullptr)
  {
    int aWidth = 0, aHeight = 0;
    glfwGetWindowPos (myGlfwWindow, &myXLeft, &myYTop);
    glfwGetWindowSize(myGlfwWindow, &aWidth, &aHeight);
    myXRight  = myXLeft + aWidth;
    myYBottom = myYTop + aHeight;

  #if !defined(_WIN32) && !defined(__APPLE__)
    myDisplay = new Aspect_DisplayConnection ((Aspect_XDisplay* )glfwGetX11Display());
  #endif
  }
}

// ================================================================
// Function : Close
// Purpose  :
// ================================================================
void Window::Close()
{
  if (myGlfwWindow != nullptr)
  {
    glfwDestroyWindow (myGlfwWindow);
    myGlfwWindow = nullptr;
  }
}

// ================================================================
// Function : NativeHandle
// Purpose  :
// ================================================================
Aspect_Drawable Window::NativeHandle() const
{
  if (myGlfwWindow == nullptr)
  {
    return 0;
  }
#if defined (__APPLE__)
  return (Aspect_Drawable)glfwGetCocoaWindow (myGlfwWindow);
#elif defined (_WIN32)
  return (Aspect_Drawable)glfwGetWin32Window (myGlfwWindow);
#else
  return (Aspect_Drawable)glfwGetX11Window (myGlfwWindow);
#endif
}

// ================================================================
// Function : NativeGlContext
// Purpose  :
// ================================================================
Aspect_RenderingContext Window::NativeGlContext() const
{
  if (myGlfwWindow == nullptr)
  {
    return nullptr;
  }
#if defined (__APPLE__)
  return (NSOpenGLContext*)glfwGetNSGLContext (myGlfwWindow);
#elif defined (_WIN32)
  return glfwGetWGLContext (myGlfwWindow);
#else
  return glfwGetGLXContext (myGlfwWindow);
#endif
}

// ================================================================
// Function : IsMapped
// Purpose  :
// ================================================================
Standard_Boolean Window::IsMapped() const
{
  if (myGlfwWindow == nullptr)
  {
    return Standard_False;
  }
  return glfwGetWindowAttrib (myGlfwWindow, GLFW_VISIBLE) != 0;
}

// ================================================================
// Function : Map
// Purpose  :
// ================================================================
void Window::Map() const
{
  if (myGlfwWindow == nullptr)
  {
    return;
  }
  glfwShowWindow (myGlfwWindow);
}

// ================================================================
// Function : Unmap
// Purpose  :
// ================================================================
void Window::Unmap() const
{
  if (myGlfwWindow == nullptr)
  {
    return;
  }
  glfwHideWindow (myGlfwWindow);
}

// ================================================================
// Function : DoResize
// Purpose  :
// ================================================================
Aspect_TypeOfResize Window::DoResize()
{
  if (myGlfwWindow == nullptr)
  {
    return Aspect_TOR_UNKNOWN;
  }
  if (glfwGetWindowAttrib (myGlfwWindow, GLFW_VISIBLE) == 1)
  {
    int anXPos = 0, anYPos = 0, aWidth = 0, aHeight = 0;
    glfwGetWindowPos (myGlfwWindow, &anXPos, &anYPos);
    glfwGetWindowSize(myGlfwWindow, &aWidth, &aHeight);
    myXLeft   = anXPos;
    myXRight  = anXPos + aWidth;
    myYTop    = anYPos;
    myYBottom = anYPos + aHeight;
  }
  return Aspect_TOR_UNKNOWN;
}

// ================================================================
// Function : CursorPosition
// Purpose  :
// ================================================================
Graphic3d_Vec2i Window::CursorPosition() const
{
  if (myGlfwWindow == nullptr)
  {
    return Graphic3d_Vec2i(0, 0);
  }
  Graphic3d_Vec2d aPos;
  glfwGetCursorPos (myGlfwWindow, &aPos.x(), &aPos.y());
  return Graphic3d_Vec2i ((int )aPos.x(), (int )aPos.y());
}
