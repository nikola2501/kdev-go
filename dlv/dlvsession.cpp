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

#include "dlvsession.h"

#include "dapclient.h"
#include "dlvbreakpointcontroller.h"
#include "dlvdebug.h"
#include "dlvframestackmodel.h"
#include "dlvvariablecontroller.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QStandardPaths>
#include <QTimer>

using namespace KDevelop;

namespace dlv {

DebugSession::DebugSession(QObject* parent)
    : IDebugSession()
    , m_client(new DapClient(this))
    , m_breakpointController(new BreakpointController(this))
    , m_frameStackModel(new FrameStackModel(this))
    , m_variableController(new VariableController(this))
{
    setParent(parent);
    connect(m_client, &DapClient::eventReceived, this, &DebugSession::onEvent);
    connect(m_client, &DapClient::processExited, this, &DebugSession::closeSession);
    connect(m_client, &DapClient::failed, this, [this](const QString& error) {
        emit outputLine(QStringLiteral("delve: ") + error);
        closeSession();
    });
}

DebugSession::~DebugSession() = default;

IDebugSession::DebuggerState DebugSession::state() const { return m_state; }
IBreakpointController* DebugSession::breakpointController() const { return m_breakpointController; }
IVariableController* DebugSession::variableController() const { return m_variableController; }
IFrameStackModel* DebugSession::frameStackModel() const { return m_frameStackModel; }

void DebugSession::setSessionState(DebuggerState state)
{
    if(m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
    if(state == PausedState)
    {
        raiseEvent(program_state_changed);
        raiseEvent(debugger_ready);
    }
    else if(state == ActiveState)
    {
        clearCurrentPosition();
        raiseEvent(program_running);
    }
    else if(state == EndedState)
    {
        raiseEvent(program_exited);
        raiseEvent(debugger_exited);
    }
}

void DebugSession::startDebugging(const QString& executable, const QStringList& arguments, const QString& workingDirectory)
{
    m_executable = executable;
    m_arguments = arguments;
    m_workingDirectory = workingDirectory;

    const QString dlv = QStandardPaths::findExecutable(QStringLiteral("dlv"));
    if(dlv.isEmpty())
    {
        emit outputLine(QStringLiteral("delve (dlv) executable not found in PATH - install it with: go install github.com/go-delve/delve/cmd/dlv@latest"));
        closeSession();
        return;
    }

    setSessionState(StartingState);
    connect(m_client, &DapClient::ready, this, [this]() {
        QJsonObject args{
            {QStringLiteral("clientID"), QStringLiteral("kdevelop")},
            {QStringLiteral("clientName"), QStringLiteral("KDevelop")},
            {QStringLiteral("adapterID"), QStringLiteral("dlv")},
            {QStringLiteral("linesStartAt1"), true},
            {QStringLiteral("columnsStartAt1"), true},
            {QStringLiteral("pathFormat"), QStringLiteral("path")},
        };
        m_client->sendRequest(QStringLiteral("initialize"), args, [this](bool success, const QJsonObject&, const QString& message) {
            if(!success)
            {
                emit outputLine(QStringLiteral("delve: initialize failed: ") + message);
                stopDebugger();
                return;
            }
            //"debug" mode lets delve build the package with the right flags itself
            //when given a source directory; a prebuilt binary is debugged with "exec"
            const bool isDirectory = QFileInfo(m_executable).isDir();
            QJsonObject launch{
                {QStringLiteral("mode"), isDirectory ? QStringLiteral("debug") : QStringLiteral("exec")},
                {QStringLiteral("program"), m_executable},
                {QStringLiteral("args"), QJsonArray::fromStringList(m_arguments)},
                {QStringLiteral("cwd"), m_workingDirectory},
                {QStringLiteral("stopOnEntry"), false},
            };
            m_client->sendRequest(QStringLiteral("launch"), launch, [this](bool success, const QJsonObject&, const QString& message) {
                if(!success)
                {
                    emit outputLine(QStringLiteral("delve: launch failed: ") + message);
                    stopDebugger();
                    return;
                }
                setSessionState(ActiveState);
            });
        });
    });
    m_client->start(dlv, workingDirectory);
}

void DebugSession::onEvent(const QString& event, const QJsonObject& body)
{
    if(event == QLatin1String("initialized"))
    {
        //breakpoints must be set between "initialized" and "configurationDone"
        m_breakpointController->sendBreakpoints();
        m_client->sendRequest(QStringLiteral("configurationDone"), {});
        m_configurationDone = true;
    }
    else if(event == QLatin1String("stopped"))
    {
        onStopped(body);
    }
    else if(event == QLatin1String("continued"))
    {
        setSessionState(ActiveState);
    }
    else if(event == QLatin1String("output"))
    {
        const auto lines = body.value(QLatin1String("output")).toString();
        const auto lineList = lines.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for(const QString& line : lineList)
            emit outputLine(line);
    }
    else if(event == QLatin1String("terminated") || event == QLatin1String("exited"))
    {
        closeSession();
    }
}

void DebugSession::onStopped(const QJsonObject& body)
{
    m_currentThreadId = body.value(QLatin1String("threadId")).toInt(1);
    m_frameIds.clear();
    setSessionState(PausedState);

    const auto hitIds = body.value(QLatin1String("hitBreakpointIds")).toArray();
    for(const auto& id : hitIds)
        m_breakpointController->notifyBreakpointHit(id.toInt());

    //show the execution point: ask for the top frame of the stopped goroutine
    QJsonObject args{
        {QStringLiteral("threadId"), m_currentThreadId},
        {QStringLiteral("startFrame"), 0},
        {QStringLiteral("levels"), 1},
    };
    m_client->sendRequest(QStringLiteral("stackTrace"), args, [this](bool success, const QJsonObject& body, const QString&) {
        if(!success)
            return;
        const auto frames = body.value(QLatin1String("stackFrames")).toArray();
        if(frames.isEmpty())
            return;
        const auto frame = frames.first().toObject();
        const QString path = frame.value(QLatin1String("source")).toObject().value(QLatin1String("path")).toString();
        const int line = frame.value(QLatin1String("line")).toInt();
        if(!path.isEmpty() && line > 0)
        {
            setCurrentPosition(QUrl::fromLocalFile(path), line - 1, QString());
            emit showStepInSource(QUrl::fromLocalFile(path), line - 1, QString());
        }
    });
}

int DebugSession::frameId(int threadNumber, int frameNumber) const
{
    const auto ids = m_frameIds.value(threadNumber);
    return frameNumber >= 0 && frameNumber < ids.size() ? ids.at(frameNumber) : -1;
}

void DebugSession::cacheFrameIds(int threadNumber, const QVector<int>& ids)
{
    m_frameIds.insert(threadNumber, ids);
}

void DebugSession::run()
{
    m_client->sendRequest(QStringLiteral("continue"), {{QStringLiteral("threadId"), m_currentThreadId}});
    setSessionState(ActiveState);
}

void DebugSession::sendStepRequest(const QString& command, const QString& granularity)
{
    QJsonObject args{{QStringLiteral("threadId"), m_currentThreadId}};
    if(!granularity.isEmpty())
        args.insert(QStringLiteral("granularity"), granularity);
    m_client->sendRequest(command, args);
    setSessionState(ActiveState);
}

void DebugSession::stepOver() { sendStepRequest(QStringLiteral("next")); }
void DebugSession::stepInto() { sendStepRequest(QStringLiteral("stepIn")); }
void DebugSession::stepOut()  { sendStepRequest(QStringLiteral("stepOut")); }
void DebugSession::stepOverInstruction() { sendStepRequest(QStringLiteral("next"), QStringLiteral("instruction")); }
void DebugSession::stepIntoInstruction() { sendStepRequest(QStringLiteral("stepIn"), QStringLiteral("instruction")); }

void DebugSession::runToCursor()
{
    //DAP has no first-class run-to-cursor; not implemented yet
    emit outputLine(QStringLiteral("delve: run to cursor is not supported yet"));
}

void DebugSession::jumpToCursor()
{
    emit outputLine(QStringLiteral("delve: jump to cursor is not supported"));
}

void DebugSession::interruptDebugger()
{
    m_client->sendRequest(QStringLiteral("pause"), {{QStringLiteral("threadId"), m_currentThreadId}});
}

void DebugSession::restartDebugger()
{
}

void DebugSession::stopDebugger()
{
    if(m_state == EndedState)
        return;
    setSessionState(StoppingState);
    m_client->sendRequest(QStringLiteral("disconnect"), {{QStringLiteral("terminateDebuggee"), true}});
    //if delve does not exit on its own, force it
    QTimer::singleShot(2000, this, [this]() {
        if(m_state != EndedState)
            killDebuggerNow();
    });
}

void DebugSession::killDebuggerNow()
{
    m_client->stop();
    closeSession();
}

void DebugSession::closeSession()
{
    if(m_state == EndedState)
        return;
    m_client->stop();
    setSessionState(EndedState);
    emit finished();
}

}
