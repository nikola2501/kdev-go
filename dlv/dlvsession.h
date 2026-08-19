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

#ifndef GOLANG_DLVSESSION_H
#define GOLANG_DLVSESSION_H

#include <debugger/interfaces/idebugsession.h>

#include <QHash>
#include <QJsonObject>
#include <QStringList>
#include <QUrl>

namespace dlv {

class DapClient;
class BreakpointController;
class FrameStackModel;
class VariableController;

class DebugSession : public KDevelop::IDebugSession
{
    Q_OBJECT
public:
    explicit DebugSession(QObject* parent = nullptr);
    ~DebugSession() override;

    DebuggerState state() const override;
    bool restartAvaliable() const override { return false; }

    KDevelop::IBreakpointController* breakpointController() const override;
    KDevelop::IVariableController* variableController() const override;
    KDevelop::IFrameStackModel* frameStackModel() const override;

    /** Launch @p executable with delve. @p buildFlags style caveats: the binary
     *  should be built with -gcflags 'all=-N -l' for a good experience. */
    void startDebugging(const QString& executable, const QStringList& arguments, const QString& workingDirectory);

    DapClient* client() const { return m_client; }
    int currentThreadId() const { return m_currentThreadId; }
    /** DAP frame id for a (thread, frame-number) pair, or -1. */
    int frameId(int threadNumber, int frameNumber) const;
    void cacheFrameIds(int threadNumber, const QVector<int>& ids);

public Q_SLOTS:
    void restartDebugger() override;
    void stopDebugger() override;
    void killDebuggerNow() override;
    void interruptDebugger() override;
    void run() override;
    void runToCursor() override;
    void jumpToCursor() override;
    void stepOver() override;
    void stepIntoInstruction() override;
    void stepInto() override;
    void stepOverInstruction() override;
    void stepOut() override;

Q_SIGNALS:
    void outputLine(const QString& line);
    void finished();

private:
    void setSessionState(DebuggerState state);
    void onEvent(const QString& event, const QJsonObject& body);
    void onStopped(const QJsonObject& body);
    void sendStepRequest(const QString& command, const QString& granularity = QString());
    void closeSession();

    DapClient* m_client;
    BreakpointController* m_breakpointController;
    FrameStackModel* m_frameStackModel;
    VariableController* m_variableController;

    DebuggerState m_state = NotStartedState;
    int m_currentThreadId = -1;
    QHash<int, QVector<int>> m_frameIds; //thread number -> DAP frame ids per frame nr
    bool m_configurationDone = false;
    QString m_executable;
    QStringList m_arguments;
    QString m_workingDirectory;
};

}

#endif
