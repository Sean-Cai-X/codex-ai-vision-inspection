#ifndef _Window_Header
#define _Window_Header

#include <Aspect_DisplayConnection.hxx>
#include <Aspect_RenderingContext.hxx>
#include <Aspect_Window.hxx>
#include <Graphic3d_Vec.hxx>
#include <TCollection_AsciiString.hxx>


struct GLFWwindow;

//! GLFWwindow wrapper implementing Aspect_Window interface.
class Window : public Aspect_Window
{
  DEFINE_STANDARD_RTTI_INLINE(Window, Aspect_Window)
public:
  //! Main constructor.
  Window (int theWidth, int theHeight, const TCollection_AsciiString& theTitle);

  //! Close the window.
  virtual ~Window() { Close(); }

  //! Close the window.
  void Close();

  //! Return X Display connection.
  const Handle(Aspect_DisplayConnection)& GetDisplay() const { return myDisplay; }

  //! Return GLFW window.
  GLFWwindow* getGlfwWindow() { return myGlfwWindow; }

  //! Return native OpenGL context.
  Aspect_RenderingContext NativeGlContext() const;

  //! Return cursor position.
  Graphic3d_Vec2i CursorPosition() const;

public:

  //! Returns native Window handle
  virtual Aspect_Drawable NativeHandle() const Standard_OVERRIDE;

  //! Returns parent of native Window handle.
  virtual Aspect_Drawable NativeParentHandle() const Standard_OVERRIDE { return 0; }

  //! Applies the resizing to the window <me>
  virtual Aspect_TypeOfResize DoResize() Standard_OVERRIDE;

  //! Returns True if the window <me> is opened and False if the window is closed.
  virtual Standard_Boolean IsMapped() const Standard_OVERRIDE;

  //! Apply the mapping change to the window <me> and returns TRUE if the window is mapped at screen.
  virtual Standard_Boolean DoMapping() const Standard_OVERRIDE { return Standard_True; }

  //! Opens the window <me>.
  virtual void Map() const Standard_OVERRIDE;

  //! Closes the window <me>.
  virtual void Unmap() const Standard_OVERRIDE;

  virtual void Position (Standard_Integer& theX1, Standard_Integer& theY1,
                         Standard_Integer& theX2, Standard_Integer& theY2) const Standard_OVERRIDE
  {
    theX1 = myXLeft;
    theX2 = myXRight;
    theY1 = myYTop;
    theY2 = myYBottom;
  }

  //! Returns The Window RATIO equal to the physical WIDTH/HEIGHT dimensions.
  virtual Standard_Real Ratio() const Standard_OVERRIDE
  {
    return Standard_Real (myXRight - myXLeft) / Standard_Real (myYBottom - myYTop);
  }

  //! Return window size.
  virtual void Size (Standard_Integer& theWidth, Standard_Integer& theHeight) const Standard_OVERRIDE
  {
    theWidth  = myXRight - myXLeft;
    theHeight = myYBottom - myYTop;
  }

  virtual Aspect_FBConfig NativeFBConfig() const Standard_OVERRIDE { return NULL; }

protected:
  Handle(Aspect_DisplayConnection) myDisplay;
  GLFWwindow*      myGlfwWindow;
  Standard_Integer myXLeft;
  Standard_Integer myYTop;
  Standard_Integer myXRight;
  Standard_Integer myYBottom;




};

#endif // _Window_Header
