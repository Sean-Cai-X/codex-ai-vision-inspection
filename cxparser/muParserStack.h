

#ifndef MU_PARSER_STACK_H
#define MU_PARSER_STACK_H

#include <cassert>
#include <string>
#include <stack>
#include <queue>

#include "muParserError.h"
#include "muParserToken.h"

namespace mu
{

template <typename TValueType>

class ParserStack
{
  private:

    typedef std::deque<TValueType> impl_type;
    impl_type m_Stack;

  public:

     ParserStack()
       :m_Stack()
     {
     }

     virtual ~ParserStack()
     {
     }

	 
	 TValueType pop()
     {
       if (empty())
         throw ParserError("stack is empty.");

       TValueType el = top();
       m_Stack.pop_back();
       return el;
     }

     
     void push(const TValueType& a_Val)
     {

       m_Stack.push_back(a_Val);
     }

     unsigned size() const
     {
       return (unsigned)m_Stack.size();
     }

     bool empty() const
     {
       return m_Stack.size()==0;
     }

     TValueType& top()
     {
         return m_Stack[size()-1];

     }

     TValueType& operator[](unsigned a_iTdx)
     {
        return	m_Stack[a_iTdx];
     }

    TValueType* GetAt(unsigned a_iTdx)
    {
       return &m_Stack[a_iTdx];
    }

	 void Assign(const ParserStack &a_stack)
	 {

	 }

	void clear()
	{
        m_Stack.clear();

	}
};
}

#endif
