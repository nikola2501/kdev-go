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

#ifndef GOLANG_DLVLAUNCHER_H
#define GOLANG_DLVLAUNCHER_H

#include <interfaces/ilauncher.h>
#include <outputview/outputjob.h>

class IExecutePlugin;

namespace dlv {

class DebugSession;

/** Job wrapping one delve debug session; finishes when the session ends. */
class DebugJob : public KDevelop::OutputJob
{
    Q_OBJECT
public:
    DebugJob(KDevelop::ILaunchConfiguration* config, IExecutePlugin* execute, QObject* parent = nullptr);
    void start() override;

protected:
    bool doKill() override;

private:
    DebugSession* m_session;
    QString m_executable;
    QStringList m_arguments;
    QString m_workingDirectory;
    QString m_error;
};

class Launcher : public KDevelop::ILauncher
{
public:
    explicit Launcher(IExecutePlugin* execute);

    QString id() override;
    QString name() const override;
    QString description() const override;
    QStringList supportedModes() const override;
    QList<KDevelop::LaunchConfigurationPageFactory*> configPages() const override;
    KJob* start(const QString& launchMode, KDevelop::ILaunchConfiguration* config) override;

private:
    IExecutePlugin* m_execute;
};

}

#endif
