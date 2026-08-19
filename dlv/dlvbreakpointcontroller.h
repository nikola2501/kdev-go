/*************************************************************************************
*  Copyright (C) 2026 by Nikola <nikolam2501@gmail.com>                             *
*                                                                                   *
*  This program is free software; you can redistribute it and/or                    *
*  modify it under the terms of the GNU General Public License                      *
*  as published by the Free Software Foundation; either version 2                   *
*  of the License, or (at your option) any later version.                           *
*                                                                                   *
*  This program is distributed in the hope that it will be useful,                  *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of                   *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                    *
*  GNU General Public License for more details.                                     *
*                                                                                   *
*  You should have received a copy of the GNU General Public License                *
*  along with this program; if not, write to the Free Software                      *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA   *
*************************************************************************************/

#ifndef GOLANG_DLVBREAKPOINTCONTROLLER_H
#define GOLANG_DLVBREAKPOINTCONTROLLER_H

#include <debugger/interfaces/ibreakpointcontroller.h>

#include <QHash>
#include <QUrl>

namespace dlv {

class DebugSession;

/**
 * Synchronizes KDevelop's breakpoint model with delve. DAP replaces the
 * whole list per file with every setBreakpoints request, so any change
 * re-sends all breakpoints of the affected file.
 */
class BreakpointController : public KDevelop::IBreakpointController
{
    Q_OBJECT
public:
    explicit BreakpointController(DebugSession* session);

    /** Send all breakpoints; used during launch (initialized event). */
    void sendBreakpoints();
    /** Map a DAP breakpoint id from a stopped event to a model row hit. */
    void notifyBreakpointHit(int dapId);

private:
    void sendMaybe(KDevelop::Breakpoint* breakpoint) override;
    void sendForFile(const QUrl& url);
    DebugSession* session() const;

    QHash<int, int> m_dapIdToRow;
};

}

#endif
