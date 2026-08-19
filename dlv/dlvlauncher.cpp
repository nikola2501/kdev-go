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

#include "dlvlauncher.h"

#include "dlvsession.h"

#include <execute/iexecuteplugin.h>
#include <interfaces/icore.h>
#include <interfaces/idebugcontroller.h>
#include <interfaces/ilaunchconfiguration.h>
#include <interfaces/iuicontroller.h>
#include <outputview/outputmodel.h>

#include <util/executecompositejob.h>

#include <KLocalizedString>

#include <QFileInfo>

using namespace KDevelop;

namespace dlv {

DebugJob::DebugJob(ILaunchConfiguration* config, IExecutePlugin* execute, QObject* parent)
    : OutputJob(parent)
    , m_session(new DebugSession(this))
{
    setCapabilities(Killable);
    setStandardToolView(IOutputView::DebugView);
    setBehaviours(IOutputView::Behaviours(IOutputView::AllowUserClose) | IOutputView::AutoScroll);
    setTitle(config->name());
    setObjectName(config->name());

    m_executable = execute->executable(config, m_error).toLocalFile();
    if(m_error.isEmpty())
        m_arguments = execute->arguments(config, m_error);
    m_workingDirectory = execute->workingDirectory(config).toLocalFile();
    if(m_workingDirectory.isEmpty())
        m_workingDirectory = QFileInfo(m_executable).absolutePath();
}

void DebugJob::start()
{
    if(!m_error.isEmpty())
    {
        setError(KJob::UserDefinedError);
        setErrorText(m_error);
        emitResult();
        return;
    }

    auto* model = new OutputModel(this);
    setModel(model);
    startOutput();

    connect(m_session, &DebugSession::outputLine, model, [model](const QString& line) {
        model->appendLine(line);
    });
    connect(m_session, &DebugSession::finished, this, [this]() {
        emitResult();
    });

    ICore::self()->debugController()->addSession(m_session);
    m_session->startDebugging(m_executable, m_arguments, m_workingDirectory);
}

bool DebugJob::doKill()
{
    m_session->stopDebugger();
    return true;
}

Launcher::Launcher(IExecutePlugin* execute)
    : m_execute(execute)
{
}

QString Launcher::id() { return QStringLiteral("dlvdebug"); }
QString Launcher::name() const { return i18n("Delve"); }
QString Launcher::description() const { return i18n("Debug a Go application with Delve"); }
QStringList Launcher::supportedModes() const { return {QStringLiteral("debug")}; }
QList<LaunchConfigurationPageFactory*> Launcher::configPages() const { return {}; }

KJob* Launcher::start(const QString& launchMode, ILaunchConfiguration* config)
{
    if(launchMode != QLatin1String("debug"))
        return nullptr;
    if(!ICore::self()->debugController()->canAddSession(
            i18n("A program is already being debugged. Do you want to abort the "
                 "currently running debug session and continue?")))
        return nullptr;
    auto* job = new DebugJob(config, m_execute);
    if(auto* dependency = m_execute->dependencyJob(config))
    {
        //run the configured pre-launch dependency (e.g. build) first
        auto* sequence = new KDevelop::ExecuteCompositeJob(nullptr, {dependency, job});
        sequence->setObjectName(job->objectName());
        return sequence;
    }
    return job;
}

}
