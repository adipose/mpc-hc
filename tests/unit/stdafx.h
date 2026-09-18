/*
 * (C) 2026 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include "../../src/DSUtil/SharedInclude.h"

#define WIN32_LEAN_AND_MEAN                 // Exclude rarely-used stuff from Windows headers
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN                        // Exclude rarely-used stuff from Windows headers
#endif

#include <afx.h>
#include <afxwin.h>                         // MFC core and standard components
#include <atlcoll.h>
#include <atlpath.h>

#include "BaseClasses/streams.h"

#include "../../src/DSUtil/DSUtil.h"

// STSStyle.h lays its members out by USE_LIBASS but does not include the header
// that defines it; without this first, a test file sees a different STSStyle
// than Subtitles.lib was built with.
#include "../../include/mpc-hc_config.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "MpcTest.h"
#include "TestUtil.h"
