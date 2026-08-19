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

#ifndef GOLANG_DAPCLIENT_H
#define GOLANG_DAPCLIENT_H

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QProcess>

#include <functional>

class QTcpSocket;

namespace dlv {

/**
 * Minimal Debug Adapter Protocol client speaking to a `dlv dap` server
 * over TCP. Starts the process, waits for its "DAP server listening at:"
 * line, connects, and exchanges Content-Length framed JSON messages.
 */
class DapClient : public QObject
{
    Q_OBJECT
public:
    using ResponseHandler = std::function<void(bool success, const QJsonObject& body, const QString& message)>;

    explicit DapClient(QObject* parent = nullptr);
    ~DapClient() override;

    /** Start `dlv dap` and connect to it. Emits ready() or failed(). */
    void start(const QString& dlvExecutable, const QString& workingDirectory);
    /** Send a DAP request; the handler is invoked with the response. */
    void sendRequest(const QString& command, const QJsonObject& arguments, ResponseHandler handler = {});
    /** Terminate the dlv process. */
    void stop();

    /** Connect to an already running DAP server (also used by tests). */
    void connectToPort(quint16 port);

    bool isConnected() const;

Q_SIGNALS:
    void ready();
    void failed(const QString& error);
    /** Any DAP event: "stopped", "continued", "terminated", "exited", "output", "initialized"... */
    void eventReceived(const QString& event, const QJsonObject& body);
    void processExited();

private:
    void onProcessOutput();
    void onSocketData();
    void dispatch(const QJsonObject& message);

    QProcess m_process;
    QTcpSocket* m_socket = nullptr;
    QByteArray m_processOutput;
    QByteArray m_buffer;
    QHash<int, ResponseHandler> m_handlers;
    int m_sequence = 0;
    bool m_connected = false;
};

}

#endif
