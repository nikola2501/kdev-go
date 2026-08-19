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

#include "dapclient.h"
#include "dlvdebug.h"

#include <QJsonDocument>
#include <QRegularExpression>
#include <QTcpSocket>

namespace dlv {

DapClient::DapClient(QObject* parent)
    : QObject(parent)
{
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &DapClient::onProcessOutput);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if(!m_connected)
            emit failed(m_process.errorString());
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this]() {
        m_connected = false;
        emit processExited();
    });
}

DapClient::~DapClient()
{
    stop();
}

void DapClient::start(const QString& dlvExecutable, const QString& workingDirectory)
{
    m_process.setWorkingDirectory(workingDirectory);
    //port 0: delve picks a free port and prints it on the first output line
    m_process.start(dlvExecutable, {QStringLiteral("dap"), QStringLiteral("--listen=127.0.0.1:0")});
}

void DapClient::onProcessOutput()
{
    m_processOutput += m_process.readAllStandardOutput();
    if(m_connected)
        return;
    //"DAP server listening at: 127.0.0.1:38975"
    static const QRegularExpression listeningRegex(QStringLiteral("listening at: 127\\.0\\.0\\.1:(\\d+)"));
    const auto match = listeningRegex.match(QString::fromUtf8(m_processOutput));
    if(!match.hasMatch())
        return;

    connectToPort(match.captured(1).toUShort());
}

void DapClient::connectToPort(quint16 port)
{
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &DapClient::onSocketData);
    connect(m_socket, &QTcpSocket::connected, this, [this]() {
        m_connected = true;
        emit ready();
    });
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if(!m_connected)
            emit failed(m_socket->errorString());
    });
    m_socket->connectToHost(QStringLiteral("127.0.0.1"), port);
}

bool DapClient::isConnected() const
{
    return m_connected;
}

void DapClient::sendRequest(const QString& command, const QJsonObject& arguments, ResponseHandler handler)
{
    if(!m_connected)
        return;
    const int seq = ++m_sequence;
    QJsonObject request{
        {QStringLiteral("seq"), seq},
        {QStringLiteral("type"), QStringLiteral("request")},
        {QStringLiteral("command"), command},
        {QStringLiteral("arguments"), arguments},
    };
    if(handler)
        m_handlers.insert(seq, std::move(handler));

    const QByteArray payload = QJsonDocument(request).toJson(QJsonDocument::Compact);
    m_socket->write("Content-Length: " + QByteArray::number(payload.size()) + "\r\n\r\n" + payload);
}

void DapClient::onSocketData()
{
    m_buffer += m_socket->readAll();
    while(true)
    {
        const int headerEnd = m_buffer.indexOf("\r\n\r\n");
        if(headerEnd == -1)
            return;
        const QByteArray header = m_buffer.left(headerEnd);
        const int lengthIndex = header.indexOf("Content-Length:");
        if(lengthIndex == -1)
        {
            qCWarning(DLV) << "malformed DAP header" << header;
            m_buffer.remove(0, headerEnd + 4);
            continue;
        }
        const int length = header.mid(lengthIndex + 15).trimmed().toInt();
        if(m_buffer.size() < headerEnd + 4 + length)
            return; //incomplete message
        const QByteArray payload = m_buffer.mid(headerEnd + 4, length);
        m_buffer.remove(0, headerEnd + 4 + length);
        dispatch(QJsonDocument::fromJson(payload).object());
    }
}

void DapClient::dispatch(const QJsonObject& message)
{
    const QString type = message.value(QLatin1String("type")).toString();
    if(type == QLatin1String("response"))
    {
        const int requestSeq = message.value(QLatin1String("request_seq")).toInt();
        auto handler = m_handlers.take(requestSeq);
        if(handler)
            handler(message.value(QLatin1String("success")).toBool(),
                    message.value(QLatin1String("body")).toObject(),
                    message.value(QLatin1String("message")).toString());
    }
    else if(type == QLatin1String("event"))
    {
        emit eventReceived(message.value(QLatin1String("event")).toString(),
                           message.value(QLatin1String("body")).toObject());
    }
}

void DapClient::stop()
{
    m_connected = false;
    if(m_socket)
        m_socket->disconnectFromHost();
    if(m_process.state() != QProcess::NotRunning)
    {
        m_process.terminate();
        if(!m_process.waitForFinished(1000))
            m_process.kill();
    }
}

}
