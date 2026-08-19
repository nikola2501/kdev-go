/* KDevelop go build support
 *
 * Copyright 2017 Mikhail Ivchenko <ematirov@gmail.com>
 * Copyright 2026 Nikola <nikolam2501@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "buildjob.h"

#include <outputview/outputfilteringstrategies.h>

using namespace KDevelop;

GoBuildJob::GoBuildJob(QObject* parent, const QStringList& arguments, const QUrl& moduleRoot)
    : OutputExecuteJob(parent), m_arguments(arguments)
{
    setJobName(QStringLiteral("go ") + arguments.join(QLatin1Char(' ')));
    setStandardToolView(IOutputView::BuildView);
    setBehaviours(IOutputView::AllowUserClose | IOutputView::AutoScroll);
    //go prints "path/file.go:line:col: message" relative to the working directory
    setFilteringStrategy(new CompilerFilterStrategy(moduleRoot));
    setWorkingDirectory(moduleRoot);
    setProperties(KDevelop::OutputExecuteJob::NeedWorkingDirectory | KDevelop::OutputExecuteJob::DisplayStderr | KDevelop::OutputExecuteJob::IsBuilderHint);
}

QStringList GoBuildJob::commandLine() const
{
    return QStringList{QStringLiteral("go")} + m_arguments;
}
