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

#include "stdafx.h"

#define MPCTEST_MAIN
#include "MpcTest.h"

// The libraries under test are MFC-static; give them the application object
// and module state they expect, the way an MFC console application does.
// AfxMessageBox ends up in DoMessageBox: some parsers report a syntax error
// that way, and a modal dialog nobody can see would stall the run for good.
class CTestApp : public CWinApp
{
public:
    int DoMessageBox(LPCTSTR lpszPrompt, UINT nType, UINT nIDPrompt) override {
        UNREFERENCED_PARAMETER(nType);
        UNREFERENCED_PARAMETER(nIDPrompt);
        return mpctest::OnMessageBox(lpszPrompt);
    }
};

CTestApp theApp;

int wmain(int argc, wchar_t** argv)
{
    if (!AfxWinInit(::GetModuleHandle(nullptr), nullptr, ::GetCommandLine(), 0)) {
        fprintf(stderr, "AfxWinInit failed\n");
        return 2;
    }
    return mpctest::Main(argc, argv);
}
