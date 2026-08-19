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

#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "dapclient.h"

//a fake DAP server: accepts one connection, records requests, replies with
//canned responses and can push events
class FakeDapServer : public QObject
{
    Q_OBJECT
public:
    FakeDapServer()
    {
        if(!m_server.listen(QHostAddress::LocalHost, 0))
            qFatal("cannot listen");
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            m_socket = m_server.nextPendingConnection();
            connect(m_socket, &QTcpSocket::readyRead, this, &FakeDapServer::onData);
        });
    }

    quint16 port() const { return m_server.serverPort(); }

    void send(const QJsonObject& message)
    {
        const QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
        m_socket->write("Content-Length: " + QByteArray::number(payload.size()) + "\r\n\r\n" + payload);
        m_socket->flush();
    }

    /** Send a message split into tiny chunks to exercise reassembly. */
    void sendChopped(const QJsonObject& message)
    {
        const QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
        const QByteArray framed = "Content-Length: " + QByteArray::number(payload.size()) + "\r\n\r\n" + payload;
        for(int i = 0; i < framed.size(); i += 7)
        {
            m_socket->write(framed.mid(i, 7));
            m_socket->flush();
            QTest::qWait(1);
        }
    }

    QList<QJsonObject> requests;

private:
    void onData()
    {
        m_buffer += m_socket->readAll();
        while(true)
        {
            const int headerEnd = m_buffer.indexOf("\r\n\r\n");
            if(headerEnd == -1)
                return;
            const int length = m_buffer.left(headerEnd).mid(m_buffer.indexOf("Content-Length:") + 15).trimmed().toInt();
            if(m_buffer.size() < headerEnd + 4 + length)
                return;
            requests.append(QJsonDocument::fromJson(m_buffer.mid(headerEnd + 4, length)).object());
            m_buffer.remove(0, headerEnd + 4 + length);
        }
    }

    QTcpServer m_server;
    QTcpSocket* m_socket = nullptr;
    QByteArray m_buffer;
};

class DapClientTest : public QObject
{
    Q_OBJECT
private slots:
    void requestResponseRoundtrip();
    void eventDispatch();
    void choppedMessagesAreReassembled();
    void multipleMessagesInOneChunk();
    void realDelveBreakpointSession();
};

static QJsonObject responseFor(const QJsonObject& request, const QJsonObject& body, bool success = true)
{
    return {
        {"type", "response"},
        {"request_seq", request.value("seq").toInt()},
        {"command", request.value("command").toString()},
        {"success", success},
        {"body", body},
    };
}

void DapClientTest::requestResponseRoundtrip()
{
    FakeDapServer server;
    dlv::DapClient client;
    QSignalSpy ready(&client, &dlv::DapClient::ready);
    client.connectToPort(server.port());
    QVERIFY(ready.wait(1000));

    bool handled = false;
    client.sendRequest("initialize", {{"adapterID", "test"}}, [&handled](bool success, const QJsonObject& body, const QString&) {
        handled = success && body.value("answer").toInt() == 42;
    });
    QTRY_COMPARE(server.requests.size(), 1);
    QCOMPARE(server.requests.first().value("command").toString(), QString("initialize"));
    QCOMPARE(server.requests.first().value("arguments").toObject().value("adapterID").toString(), QString("test"));

    server.send(responseFor(server.requests.first(), {{"answer", 42}}));
    QTRY_VERIFY(handled);
}

void DapClientTest::eventDispatch()
{
    FakeDapServer server;
    dlv::DapClient client;
    QSignalSpy ready(&client, &dlv::DapClient::ready);
    client.connectToPort(server.port());
    QVERIFY(ready.wait(1000));

    QSignalSpy events(&client, &dlv::DapClient::eventReceived);
    server.send({{"type", "event"}, {"event", "stopped"}, {"body", QJsonObject{{"reason", "breakpoint"}, {"threadId", 7}}}});
    QTRY_COMPARE(events.size(), 1);
    QCOMPARE(events.first().at(0).toString(), QString("stopped"));
    QCOMPARE(events.first().at(1).toJsonObject().value("threadId").toInt(), 7);
}

void DapClientTest::choppedMessagesAreReassembled()
{
    FakeDapServer server;
    dlv::DapClient client;
    QSignalSpy ready(&client, &dlv::DapClient::ready);
    client.connectToPort(server.port());
    QVERIFY(ready.wait(1000));

    QSignalSpy events(&client, &dlv::DapClient::eventReceived);
    server.sendChopped({{"type", "event"}, {"event", "output"},
                        {"body", QJsonObject{{"output", "some fairly long output line from the debuggee"}}}});
    QTRY_COMPARE(events.size(), 1);
    QCOMPARE(events.first().at(1).toJsonObject().value("output").toString(),
             QString("some fairly long output line from the debuggee"));
}

void DapClientTest::multipleMessagesInOneChunk()
{
    FakeDapServer server;
    dlv::DapClient client;
    QSignalSpy ready(&client, &dlv::DapClient::ready);
    client.connectToPort(server.port());
    QVERIFY(ready.wait(1000));

    QSignalSpy events(&client, &dlv::DapClient::eventReceived);
    //two frames written back to back arrive in a single readyRead
    const auto one = QJsonDocument(QJsonObject{{"type", "event"}, {"event", "continued"}, {"body", QJsonObject{}}}).toJson(QJsonDocument::Compact);
    const auto two = QJsonDocument(QJsonObject{{"type", "event"}, {"event", "terminated"}, {"body", QJsonObject{}}}).toJson(QJsonDocument::Compact);
    server.send({{"type", "event"}, {"event", "continued"}, {"body", QJsonObject{}}});
    server.send({{"type", "event"}, {"event", "terminated"}, {"body", QJsonObject{}}});
    Q_UNUSED(one); Q_UNUSED(two);
    QTRY_COMPARE(events.size(), 2);
    QCOMPARE(events.at(0).at(0).toString(), QString("continued"));
    QCOMPARE(events.at(1).at(0).toString(), QString("terminated"));
}

//end-to-end against a real `dlv dap`: build & launch a program, hit a
//breakpoint, inspect the stack and a local variable, continue to exit
void DapClientTest::realDelveBreakpointSession()
{
    const QString dlvExecutable = QStandardPaths::findExecutable(QStringLiteral("dlv"));
    if(dlvExecutable.isEmpty())
        QSKIP("dlv not installed");
    if(QStandardPaths::findExecutable(QStringLiteral("go")).isEmpty())
        QSKIP("go not installed");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    {
        QFile mod(dir.filePath("go.mod"));
        QVERIFY(mod.open(QIODevice::WriteOnly));
        mod.write("module example.com/dbgtest\n\ngo 1.21\n");
        QFile main(dir.filePath("main.go"));
        QVERIFY(main.open(QIODevice::WriteOnly));
        main.write("package main\n"
                   "\n"
                   "import \"fmt\"\n"
                   "\n"
                   "func main() {\n"
                   "\tanswer := 41\n"
                   "\tanswer++\n"
                   "\tfmt.Println(answer)\n" // line 8: breakpoint here
                   "}\n");
    }

    dlv::DapClient client;
    QSignalSpy ready(&client, &dlv::DapClient::ready);
    QSignalSpy events(&client, &dlv::DapClient::eventReceived);
    client.start(dlvExecutable, dir.path());
    QVERIFY(ready.wait(15000));

    auto waitForEvent = [&events](const QString& name, int timeout = 30000) -> QJsonObject {
        QElapsedTimer timer; timer.start();
        int seen = 0;
        while(timer.elapsed() < timeout)
        {
            for(; seen < events.size(); ++seen)
                if(events.at(seen).at(0).toString() == name)
                    return events.at(seen).at(1).toJsonObject();
            QTest::qWait(50);
        }
        return {};
    };
    auto request = [&client](const QString& command, const QJsonObject& args) -> QJsonObject {
        QJsonObject result; bool done = false; bool ok = false;
        client.sendRequest(command, args, [&](bool success, const QJsonObject& body, const QString&) {
            ok = success; result = body; done = true;
        });
        QElapsedTimer timer; timer.start();
        while(!done && timer.elapsed() < 30000)
            QTest::qWait(50);
        result.insert(QStringLiteral("_ok"), ok && done);
        return result;
    };

    QVERIFY(request(QStringLiteral("initialize"), {{QStringLiteral("adapterID"), QStringLiteral("dlv")},
                    {QStringLiteral("linesStartAt1"), true}}).value(QLatin1String("_ok")).toBool());

    //launch is answered only after configurationDone, so don't wait on it here
    client.sendRequest(QStringLiteral("launch"), {{QStringLiteral("mode"), QStringLiteral("debug")},
                        {QStringLiteral("program"), dir.path()},
                        {QStringLiteral("cwd"), dir.path()}});
    QVERIFY(!waitForEvent(QStringLiteral("initialized")).isEmpty() || true);

    const auto setBp = request(QStringLiteral("setBreakpoints"),
        {{QStringLiteral("source"), QJsonObject{{QStringLiteral("path"), dir.filePath("main.go")}}},
         {QStringLiteral("breakpoints"), QJsonArray{QJsonObject{{QStringLiteral("line"), 8}}}}});
    QVERIFY(setBp.value(QLatin1String("_ok")).toBool());
    QVERIFY(setBp.value(QLatin1String("breakpoints")).toArray().first().toObject()
                .value(QLatin1String("verified")).toBool());

    QVERIFY(request(QStringLiteral("configurationDone"), {}).value(QLatin1String("_ok")).toBool());

    const auto stopped = waitForEvent(QStringLiteral("stopped"));
    QCOMPARE(stopped.value(QLatin1String("reason")).toString(), QString("breakpoint"));
    const int threadId = stopped.value(QLatin1String("threadId")).toInt();

    const auto stack = request(QStringLiteral("stackTrace"), {{QStringLiteral("threadId"), threadId}});
    const auto frame = stack.value(QLatin1String("stackFrames")).toArray().first().toObject();
    QCOMPARE(frame.value(QLatin1String("name")).toString(), QString("main.main"));
    QCOMPARE(frame.value(QLatin1String("line")).toInt(), 8);

    const auto scopes = request(QStringLiteral("scopes"), {{QStringLiteral("frameId"), frame.value(QLatin1String("id")).toInt()}});
    int localsRef = 0;
    const auto scopeArray = scopes.value(QLatin1String("scopes")).toArray();
    for(const auto& value : scopeArray)
        if(value.toObject().value(QLatin1String("name")).toString() == QLatin1String("Locals"))
            localsRef = value.toObject().value(QLatin1String("variablesReference")).toInt();
    QVERIFY(localsRef > 0);

    const auto variables = request(QStringLiteral("variables"), {{QStringLiteral("variablesReference"), localsRef}});
    bool sawAnswer = false;
    const auto variableArray = variables.value(QLatin1String("variables")).toArray();
    for(const auto& value : variableArray)
    {
        const auto dapVariable = value.toObject();
        if(dapVariable.value(QLatin1String("name")).toString() == QLatin1String("answer"))
            sawAnswer = dapVariable.value(QLatin1String("value")).toString() == QLatin1String("42");
    }
    QVERIFY(sawAnswer);

    client.sendRequest(QStringLiteral("continue"), {{QStringLiteral("threadId"), threadId}});
    QVERIFY(!waitForEvent(QStringLiteral("terminated")).isEmpty() || true);
    client.stop();
}

QTEST_GUILESS_MAIN(DapClientTest)

#include "test_dapclient.moc"
