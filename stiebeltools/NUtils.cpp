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

#include "NTypes.h"
#include "NUtils.h"

namespace NUtils
{




bool IsHexDigit(char aDigit)
{
  if (('0' <= aDigit && aDigit <= '9') ||
      ('a' <= aDigit && aDigit <= 'f') ||
			('A' <= aDigit && aDigit <= 'F'))
    return true;

  return false;
}

int GetHexDigit(char aDigit)
{
  if ('0' <= aDigit && aDigit <= '9')
    return (int) aDigit - '0';

  if ('A' <= aDigit && aDigit <= 'F')
    return (int) aDigit - ('A' - 10);

  if ('a' <= aDigit && aDigit <= 'f')
    return (int) aDigit - ('a' - 10);

  return -1;
}

unsigned GetHex(const char * & Line)
{
  if (!Line)
    return 0;

  unsigned Hex = 0;

  while (IsHexDigit(*Line))
  {
    Hex = 16*Hex + GetHexDigit(*Line++);
  }
  return Hex;
}




  
unsigned GetInt(TInt64 & res, const char * Line)
{
  if (!Line)
    return 0;

  const char * ptr = Line;
  bool Ok = GetInt(ptr, res);
  if (!Ok)
    return 0;

  return (unsigned)(ptr - Line);
}

bool GetInt(const char * & Line, TInt64 & res)
{
  if (!Line)
    return false;

  bool IsHex = false;
  if (Line[0] == '0' && Line[1] == 'x')
  {
    Line += 2;
    IsHex = true;
  } else
  if (Line[0] == '$')
  {
    Line++;
    IsHex = true;
  }
  if (IsHex)
  {
    if (!IsHexDigit(Line[0]))
      return false;

    unsigned u = GetHex(Line);
    res = (int) u;
    return true;
  }

  bool neg = *Line == '-';
  if (neg)
    Line++;

  bool Ok = ('0' <= *Line && *Line <= '9');

  res = 0;
  while ('0' <= *Line && *Line <= '9')
  {
    res = 10*res + (unsigned char)(*Line) - '0';
    Line++;
  }
  if (neg)
    res = -res;

  return Ok;
}

bool GetDouble(const char * & Ptr, double & res)
{
  bool neg = *Ptr == '-';
  if (neg)
    Ptr++;

  res = 0;
  if (*Ptr < '0' || *Ptr > '9')
    return false;

  while ('0' <= *Ptr && *Ptr <= '9')
  {
    res = 10.0*res + (unsigned char)(*Ptr - '0');
    Ptr++;
  }
  if (*Ptr == '.')
  {
    double Res2 = 0;
    double quot = 1.0;

    Ptr++;
    if (*Ptr < '0' || *Ptr > '9')
      return false;

    while ('0' <= *Ptr && *Ptr <= '9')
    {
      Res2 = 10.0*Res2 + (unsigned char)(*Ptr - '0');
      quot *= 10.0;
      Ptr++;
    }
    Res2 /= quot;
    res += Res2;
  }
  if (neg)
    res = -res;

  return true;
}

 

}





