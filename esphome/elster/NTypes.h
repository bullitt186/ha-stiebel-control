/*
 *
 *  Copyright (C) 2014 Jürg Müller, CH-5524.
 *  Source: http://juerg5524.ch/list_data.php
 *  Modified by Bastian Stahmer in 2023
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see http://www.gnu.org/licenses/ .
 */

#if !defined(NTypes_H)

  #define NTypes_H

  #if defined(__BIGENDIAN__)
    #define cBigEndianMachine true
  #else
    #define cBigEndianMachine false
  #endif

  // list predefined macros:
  //   gcc -x c /dev/null -dM -E | grep 64 | sort
  //   clang -x c /dev/null -dM -E

  #if (defined(__APPLE__) || defined(__clang__)) && !defined(__MAC__)
    #define __MAC__
  #endif

  #if defined(__GNUC__) && !defined(__MINGW32__) && !defined(__LINUX__)
    #define __LINUX__
  #endif

  #if !defined(__64__) && defined(__x86_64__)
    #define __64__
  #endif

  // Embarcadero: _WIN64, __WIN32__
  // Borland: __WIN32__
  // Visual Studio: _WIN32, _MSC_VER
  // GNU C: __GNUC__
  #if !defined(__WINDOWS__) && !defined(__LINUX__)
    #if defined(__WIN64__) || defined(__WIN32__) || defined(_WIN32)
      #define __WINDOWS__
    #endif
  #endif
  #if (defined(__x86_64__) || defined(__x86_64) || defined(_WIN64)) && !defined(__64__)
    #define __64__
  #endif

  #if !defined(__ARM__) && (defined(__arm__) || defined(__linux__))
    #define __ARM__
  #endif

  #if (defined(__MAC__) || defined(__ARM__)) && !defined(__LINUX__)
    #define __LINUX__
  #endif

  #if defined(_MSC_VER) && !defined(__VC__)
    #define __VC__
  #endif

  #if (defined(__BORLANDC__) || defined(__VC__)) && !defined(__WINDOWS__)
    #define __WINDOWS__
  #endif

  #if defined(__LINUX__) || defined(__CYGWIN__) || defined(__GNUC__)
    #if defined(__64__)
      typedef signed long TInt64;
      typedef unsigned long TUInt64;
    #else
      typedef signed long long TInt64;
      typedef unsigned long long TUInt64;
    #endif
    #define GNU_LINUX
  #elif defined(__WINDOWS__)
    typedef __int64 TInt64;
    typedef unsigned __int64 TUInt64;
  #endif
  #if (_FILE_OFFSET_BITS==64) || defined(__64__)
    typedef TUInt64 TFileSize;
  #else
    typedef unsigned long TFileSize;
  #endif

  typedef TInt64 TGeneralValue;
  #if defined(__64__)
    typedef TInt64 TNativeInt;
  #else
    typedef int    TNativeInt;
  #endif

  #if !defined(__WINDOWS__) && !defined(__CYGWIN__)
    #define __stdcall
  #endif

  #if defined(__VC__)
    #define stricmp   _stricmp
    #define chmod     _chmod
    #define strnicmp  _strnicmp
    #define putenv    _putenv
  #endif

  #if defined(__LINUX__)

    #define stricmp    strcasecmp
    #define strnicmp   strncasecmp

  #endif

  #if (_FILE_OFFSET_BITS==64) && !defined(__64__) && !defined(__ARM__)
  //  #define ftell    ftello
  //  #define fseek    fseeko
    #define ftell ltell
  #endif

  #define High(A)     (sizeof(A)/sizeof(A[0]) - 1)

#ifdef __cplusplus

  template <class T> T Min(const T A, const T B)
  {
    return (A < B ? A : B);
  }

  template <class T> T Max(const T A, const T B)
  {
    return (A >= B ? A : B);
  }

  template <class T> T sqr(const T x)
  {
    return x*x;
  }

  template <class T> T Abs(const T x)
  {
    if (x < 0)
      return -x;
    else
      return x;
  }

  template <class T> void xchange(T & v, T & w)
  {
    T r;

    r = v; v = w; w = r;
  }

#endif

#endif

