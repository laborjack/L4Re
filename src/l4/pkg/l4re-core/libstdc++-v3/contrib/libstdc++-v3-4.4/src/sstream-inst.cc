// Explicit instantiation file.

// Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002, 2005, 2009
// Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Under Section 7 of GPL version 3, you are granted additional
// permissions described in the GCC Runtime Library Exception, version
// 3.1, as published by the Free Software Foundation.

// You should have received a copy of the GNU General Public License and
// a copy of the GCC Runtime Library Exception along with this program;
// see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
// <http://www.gnu.org/licenses/>.

//
// ISO C++ 14882:
//

#include <sstream>

_GLIBCXX_BEGIN_NAMESPACE(std)

  template class basic_stringbuf<char>;
  template class basic_istringstream<char>;
  template class basic_ostringstream<char>;
  template class basic_stringstream<char>;

#ifdef _GLIBCXX_USE_WCHAR_T
  template class basic_stringbuf<wchar_t>;
  template class basic_istringstream<wchar_t>; 
  template class basic_ostringstream<wchar_t>; 
  template class basic_stringstream<wchar_t>; 
#endif

_GLIBCXX_END_NAMESPACE
