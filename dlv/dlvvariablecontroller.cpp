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

#include "dlvvariablecontroller.h"

#include "dapclient.h"
#include "dlvsession.h"

#include <interfaces/icore.h>
#include <interfaces/idebugcontroller.h>
#include <debugger/interfaces/iframestackmodel.h>

#include <KTextEditor/Document>

#include <QJsonArray>
#include <QPointer>
#include <QJsonObject>

using KDevelop::ICore;

namespace dlv {

Variable::Variable(DebugSession* session, KDevelop::TreeModel* model, KDevelop::TreeItem* parent,
                   const QString& expression, const QString& display)
    : KDevelop::Variable(model, parent, expression, display)
    , m_session(session)
{
}

void Variable::updateFromDap(const QJsonObject& dapVariable)
{
    setValue(dapVariable.value(QLatin1String("value")).toString());
    setType(dapVariable.value(QLatin1String("type")).toString());
    m_variablesReference = dapVariable.value(QLatin1String("variablesReference")).toInt();
    setHasMore(m_variablesReference > 0);
    setInScope(true);
}

void Variable::attachMaybe(QObject* callback, const char* callbackMethod)
{
    const int frameId = m_session->frameId(m_session->frameStackModel()->currentThread(),
                                           m_session->frameStackModel()->currentFrame());
    QJsonObject args{
        {QStringLiteral("expression"), expression()},
        {QStringLiteral("context"), QStringLiteral("watch")},
    };
    if(frameId != -1)
        args.insert(QStringLiteral("frameId"), frameId);
    QPointer<Variable> guard(this);
    m_session->client()->sendRequest(QStringLiteral("evaluate"), args,
                                     [guard, callback, callbackMethod](bool success, const QJsonObject& body, const QString& message) {
        if(!guard)
            return;
        if(success)
        {
            guard->setValue(body.value(QLatin1String("result")).toString());
            guard->setType(body.value(QLatin1String("type")).toString());
            guard->m_variablesReference = body.value(QLatin1String("variablesReference")).toInt();
            guard->setHasMore(guard->m_variablesReference > 0);
            guard->setInScope(true);
        }
        else
        {
            guard->setValue(message);
            guard->setInScope(false);
        }
        if(callback && callbackMethod)
            QMetaObject::invokeMethod(callback, callbackMethod + 1, Q_ARG(bool, success));
    });
}

void Variable::fetchMoreChildren()
{
    if(m_variablesReference <= 0)
        return;
    QPointer<Variable> guard(this);
    m_session->client()->sendRequest(QStringLiteral("variables"),
                                     {{QStringLiteral("variablesReference"), m_variablesReference}},
                                     [guard](bool success, const QJsonObject& body, const QString&) {
        if(!guard || !success)
            return;
        guard->deleteChildren();
        const auto dapVariables = body.value(QLatin1String("variables")).toArray();
        for(const auto& value : dapVariables)
        {
            const auto dapVariable = value.toObject();
            auto* child = new Variable(guard->m_session, guard->model(), guard,
                                       dapVariable.value(QLatin1String("name")).toString());
            guard->appendChild(child);
            child->updateFromDap(dapVariable);
        }
        guard->setHasMore(false);
    });
}

VariableController::VariableController(DebugSession* session)
    : IVariableController(session)
{
}

DebugSession* VariableController::session() const
{
    return static_cast<DebugSession*>(parent());
}

KDevelop::Variable* VariableController::createVariable(KDevelop::TreeModel* model, KDevelop::TreeItem* parent,
                                                       const QString& expression, const QString& display)
{
    return new Variable(session(), model, parent, expression, display);
}

KTextEditor::Range VariableController::expressionRangeUnderCursor(KTextEditor::Document* doc,
                                                                  const KTextEditor::Cursor& cursor)
{
    const QString line = doc->line(cursor.line());
    const auto isExpressionChar = [](QChar c) { return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('.'); };
    int start = cursor.column();
    int end = cursor.column();
    if(start >= line.size() || !isExpressionChar(line.at(start)))
        return {};
    while(start > 0 && isExpressionChar(line.at(start - 1)))
        --start;
    while(end < line.size() && isExpressionChar(line.at(end)))
        ++end;
    return {cursor.line(), start, cursor.line(), end};
}

void VariableController::addWatch(KDevelop::Variable* variable)
{
    ICore::self()->debugController()->variableCollection()->watches()->add(variable->expression());
}

void VariableController::addWatchpoint(KDevelop::Variable*)
{
    //delve/DAP has no watchpoint support through setDataBreakpoints for Go yet
}

void VariableController::update()
{
    if(autoUpdate() & UpdateWatches)
        ICore::self()->debugController()->variableCollection()->watches()->reinstall();
    if(autoUpdate() & UpdateLocals)
        updateLocals();
}

void VariableController::updateLocals()
{
    const int frameId = session()->frameId(session()->frameStackModel()->currentThread(),
                                           session()->frameStackModel()->currentFrame());
    if(frameId == -1)
        return;
    session()->client()->sendRequest(QStringLiteral("scopes"), {{QStringLiteral("frameId"), frameId}},
                                     [this](bool success, const QJsonObject& body, const QString&) {
        if(!success)
            return;
        const auto scopes = body.value(QLatin1String("scopes")).toArray();
        for(const auto& value : scopes)
        {
            const auto scope = value.toObject();
            const QString name = scope.value(QLatin1String("name")).toString();
            //delve exposes "Arguments", "Locals" and (on demand) "Registers"
            if(name == QLatin1String("Registers"))
                continue;
            const int reference = scope.value(QLatin1String("variablesReference")).toInt();
            session()->client()->sendRequest(QStringLiteral("variables"),
                                             {{QStringLiteral("variablesReference"), reference}},
                                             [this](bool success, const QJsonObject& body, const QString&) {
                if(!success)
                    return;
                const auto dapVariables = body.value(QLatin1String("variables")).toArray();
                QStringList names;
                names.reserve(dapVariables.size());
                for(const auto& value : dapVariables)
                    names.append(value.toObject().value(QLatin1String("name")).toString());

                auto* locals = ICore::self()->debugController()->variableCollection()->locals();
                const auto variables = locals->updateLocals(names);
                for(int i = 0; i < variables.size() && i < dapVariables.size(); ++i)
                {
                    if(auto* variable = qobject_cast<Variable*>(variables.at(i)))
                        variable->updateFromDap(dapVariables.at(i).toObject());
                }
            });
        }
    });
}

}
