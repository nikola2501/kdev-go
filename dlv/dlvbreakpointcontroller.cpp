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

#include "dlvbreakpointcontroller.h"

#include "dapclient.h"
#include "dlvsession.h"

#include <debugger/breakpoint/breakpoint.h>
#include <debugger/breakpoint/breakpointmodel.h>
#include <interfaces/icore.h>
#include <interfaces/idebugcontroller.h>

#include <QJsonArray>
#include <QSet>
#include <QJsonObject>

using namespace KDevelop;

namespace dlv {

BreakpointController::BreakpointController(DebugSession* session)
    : IBreakpointController(session)
{
}

DebugSession* BreakpointController::session() const
{
    return static_cast<DebugSession*>(parent());
}

void BreakpointController::sendMaybe(Breakpoint* breakpoint)
{
    if(!session()->client()->isConnected())
        return;
    if(breakpoint->kind() != Breakpoint::CodeBreakpoint || breakpoint->url().isEmpty())
        return;
    sendForFile(breakpoint->url());
}

void BreakpointController::sendBreakpoints()
{
    QSet<QUrl> files;
    const auto* model = breakpointModel();
    for(Breakpoint* breakpoint : model->breakpoints())
    {
        if(breakpoint->kind() == Breakpoint::CodeBreakpoint && !breakpoint->url().isEmpty())
            files.insert(breakpoint->url());
    }
    for(const QUrl& url : std::as_const(files))
        sendForFile(url);
}

void BreakpointController::sendForFile(const QUrl& url)
{
    auto* model = breakpointModel();
    QJsonArray dapBreakpoints;
    QVector<int> rows;
    const auto breakpoints = model->breakpoints();
    for(int row = 0; row < breakpoints.size(); ++row)
    {
        Breakpoint* breakpoint = breakpoints.at(row);
        if(breakpoint->kind() != Breakpoint::CodeBreakpoint || breakpoint->url() != url || !breakpoint->enabled())
            continue;
        QJsonObject dapBreakpoint{{QStringLiteral("line"), breakpoint->line() + 1}};
        if(!breakpoint->condition().isEmpty())
            dapBreakpoint.insert(QStringLiteral("condition"), breakpoint->condition());
        dapBreakpoints.append(dapBreakpoint);
        rows.append(row);
    }

    QJsonObject args{
        {QStringLiteral("source"), QJsonObject{{QStringLiteral("path"), url.toLocalFile()}}},
        {QStringLiteral("breakpoints"), dapBreakpoints},
    };
    session()->client()->sendRequest(QStringLiteral("setBreakpoints"), args,
                                     [this, rows](bool success, const QJsonObject& body, const QString& message) {
        const auto result = body.value(QLatin1String("breakpoints")).toArray();
        for(int i = 0; i < rows.size(); ++i)
        {
            const int row = rows.at(i);
            if(!success || i >= result.size())
            {
                updateErrorText(row, message.isEmpty() ? QStringLiteral("setBreakpoints failed") : message);
                continue;
            }
            const auto dapBreakpoint = result.at(i).toObject();
            if(dapBreakpoint.value(QLatin1String("verified")).toBool())
            {
                m_dapIdToRow.insert(dapBreakpoint.value(QLatin1String("id")).toInt(-1), row);
                updateState(row, Breakpoint::CleanState);
            }
            else
                updateErrorText(row, dapBreakpoint.value(QLatin1String("message")).toString(
                                         QStringLiteral("could not set breakpoint")));
        }
    });
}

void BreakpointController::notifyBreakpointHit(int dapId)
{
    const auto row = m_dapIdToRow.constFind(dapId);
    if(row != m_dapIdToRow.constEnd())
        notifyHit(row.value(), QString());
}

}
