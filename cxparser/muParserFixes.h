

#ifndef MU_PARSER_FIXES_H
#ifdef _MSC_VER
#define MU_PARSER_FIXES_H

#ifdef __INTEL_COMPILER

#pragma warning(disable:981)

#pragma warning(disable:383)

#pragma warning(disable:1572)

#endif

#if _MSC_VER==1200

#define auto_ptr _my_auto_ptr

#undef min
#undef max

namespace std
{
  typedef ::size_t size_t;

  inline int rand(void)
  {
    return ::rand();
  }

  inline size_t strlen(const char *szMsg)
  {
    return ::strlen(szMsg);
  }

  inline int strncmp(const char *a, const char *b, size_t len)
  {
    return ::strncmp(a,b,len);
  }

  template<typename T>
  T max(T a, T b)
  {
    return (a>b) ? a : b;
  }

  template<typename T>
  T min(T a, T b)
  {
    return (a<b) ? a : b;
  }

  template<class _Ty>
  class _my_auto_ptr
  {
  public:
    typedef _Ty element_type;

	  explicit _my_auto_ptr(_Ty *_Ptr = 0)
	    :_Myptr(_Ptr)
	  {}

	  _my_auto_ptr(_my_auto_ptr<_Ty>& _Right)
	    :_Myptr(_Right.release())
	  {}

	  template<class _Other>
    operator _my_auto_ptr<_Other>()
	  {
      return (_my_auto_ptr<_Other>(*this));
	  }

	  template<class _Other>
	  _my_auto_ptr<_Ty>& operator=(_my_auto_ptr<_Other>& _Right)
	  {
      reset(_Right.release());
	    return (*this);
	  }

	 ~auto_ptr()              { delete _Myptr;    }
	  _Ty& operator*() const  { return (*_Myptr); }
	  _Ty *operator->() const	{ return (&**this);	}
	  _Ty *get() const        { return (_Myptr);	}

    _Ty *release()
    {
		  _Ty *_Tmp = _Myptr;
		  _Myptr = 0;
		  return (_Tmp);
		}

	  void reset(_Ty* _Ptr = 0)
		{
		  if (_Ptr != _Myptr)
			  delete _Myptr;
		  _Myptr = _Ptr;
    }

  private:
	    _Ty *_Myptr;
	};
}

#endif

#endif
#endif
