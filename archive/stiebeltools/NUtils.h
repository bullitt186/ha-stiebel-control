/*
 *  Copyright (C) 2023 Bastian Stahmer, based heavily on the great work of Jürg Müller, CH-5524 (see below)
 *  Copyright (C) 2014 Jürg Müller, CH-5524
 * 
 *  This file is part of ha-stiebel-control.
 *  ha-stiebel-control is free software: : you can redistribute it and/or modify
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

#if !defined(NUtils_H)

  #define NUtils_H

  #include <stdio.h>
  //#include <time.h>
  #include "NTypes.h"

  namespace NUtils
  {

 

    int GetHexDigit(char aDigit);
    unsigned GetHex(const char * & Line);
    int  GetDigit(char Digit);
    bool GetHexByte(const char * & Line, unsigned char & byte);
    bool GetInt(const char * & Line, TInt64 & res);
    bool GetUnsigned(const char * & Line, unsigned & res);
    unsigned GetInt(TInt64 & res, const char * Line);
    bool GetDouble(const char * & Ptr, double & res);

  }
#endif

