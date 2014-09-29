/*
 * VapourSynth D2V Plugin
 *
 * Copyright (c) 2012 Derek Buitenhuis
 *
 * This file is part of d2vsource.
 *
 * d2vsource is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * d2vsource is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with d2vsource; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <fstream>
#include <string>

#include "compat.hpp"

using namespace std;

/* Replacement function for getline that removes any trailing \r. */
istream& d2vgetline(istream& is, string& str)
{
    string tmp;

    str.clear();

    getline(is, tmp);

    if (tmp.size() != 0 && tmp.at(tmp.length() - 1) == 0x0D)
        tmp.erase(tmp.length() - 1);

    str = tmp;

    return is;
}
