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

#include "dlvframestackmodel.h"

#include "dapclient.h"
#include "dlvsession.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

namespace dlv {

FrameStackModel::FrameStackModel(DebugSession* session)
    : KDevelop::FrameStackModel(session)
{
}

DebugSession* FrameStackModel::session() const
{
    return static_cast<DebugSession*>(KDevelop::FrameStackModel::session());
}

void FrameStackModel::fetchThreads()
{
    session()->client()->sendRequest(QStringLiteral("threads"), {},
                                     [this](bool success, const QJsonObject& body, const QString&) {
        if(!success)
            return;
        QVector<ThreadItem> threads;
        const auto dapThreads = body.value(QLatin1String("threads")).toArray();
        threads.reserve(dapThreads.size());
        for(const auto& value : dapThreads)
        {
            const auto dapThread = value.toObject();
            threads.append({dapThread.value(QLatin1String("id")).toInt(),
                            dapThread.value(QLatin1String("name")).toString()});
        }
        setThreads(threads);
        setCurrentThread(session()->currentThreadId());
    });
}

void FrameStackModel::fetchFrames(int threadNumber, int from, int to)
{
    QJsonObject args{
        {QStringLiteral("threadId"), threadNumber},
        {QStringLiteral("startFrame"), from},
        {QStringLiteral("levels"), to - from + 1},
    };
    session()->client()->sendRequest(QStringLiteral("stackTrace"), args,
                                     [this, threadNumber, from](bool success, const QJsonObject& body, const QString&) {
        if(!success)
            return;
        QVector<FrameItem> frames;
        QVector<int> frameIds = from == 0 ? QVector<int>() : QVector<int>(from, -1);
        const auto dapFrames = body.value(QLatin1String("stackFrames")).toArray();
        int nr = from;
        for(const auto& value : dapFrames)
        {
            const auto dapFrame = value.toObject();
            FrameItem frame;
            frame.nr = nr++;
            frame.name = dapFrame.value(QLatin1String("name")).toString();
            frame.file = QUrl::fromLocalFile(
                dapFrame.value(QLatin1String("source")).toObject().value(QLatin1String("path")).toString());
            frame.line = dapFrame.value(QLatin1String("line")).toInt() - 1;
            frames.append(frame);
            frameIds.append(dapFrame.value(QLatin1String("id")).toInt(-1));
        }
        session()->cacheFrameIds(threadNumber, frameIds);
        if(from == 0)
            setFrames(threadNumber, frames);
        else
            insertFrames(threadNumber, frames);
        const int total = body.value(QLatin1String("totalFrames")).toInt(frames.size());
        setHasMoreFrames(threadNumber, total > from + frames.size());
    });
}

}
