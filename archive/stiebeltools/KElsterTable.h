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

#if !defined(KElsterTable_H)

  #define KElsterTable_H

  #include "ElsterTable.h"

  const ElsterIndex * GetElsterIndex(unsigned short Index);
  const ElsterIndex * GetElsterIndex(const char * str);
  ElsterType GetElsterType(const char * str);
  void SetValueType(char * Val, unsigned char Type, unsigned short Value);
  void SetDoubleType(char * Val, unsigned char Type, double Value);
  const char * ElsterTypeToName(unsigned Type);
  int TranslateString(const char * & str, unsigned char elster_type);


#endif

