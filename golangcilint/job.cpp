/* KDevelop golangci-lint support
 *
 * Copyright 2017 Mikhail Ivchenko <ematirov@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "job.h"

#include <klocalizedstring.h>
#include <kmessagebox.h>
#include <shell/problem.h>

#include <QApplication>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <language/editor/documentrange.h>

namespace GolangciLint
{


Job::Job(const QUrl &workingDirectory, const QString &path, QObject *parent)
    : KDevelop::OutputExecuteJob(parent)
    , m_timer(new QElapsedTimer)
{
    setJobName(i18n("golangci-lint Analysis"));

    setCapabilities(KJob::Killable);
    setStandardToolView(KDevelop::IOutputView::AnalyzeView);
    setBehaviours(KDevelop::IOutputView::AutoScroll);

    setProperties(KDevelop::OutputExecuteJob::JobProperty::DisplayStdout);
    setProperties(KDevelop::OutputExecuteJob::JobProperty::DisplayStderr);
    setProperties(KDevelop::OutputExecuteJob::JobProperty::PostProcessOutput);

    setWorkingDirectory(workingDirectory);
    //golangci-lint's default text output is "path:line:col: message (linter)"
    //in both v1 and v2, so no output-format flag is passed
    QStringList commandLine = {"golangci-lint", "run", "--output.text.path=stdout", "--show-stats=false", path};
    *this << commandLine;
    m_projectRootPath = KDevelop::Path(workingDirectory);
}

Job::~Job()
{
    doKill();
}

void Job::postProcessStdout(const QStringList& lines)
{
    //e.g. "internal/ui/ui.go:42:2: unused variable `x` (unused)"
    static const auto problemRegex = QRegularExpression(
        QStringLiteral("^([^\\s:]+\\.go):(\\d+)(?::(\\d+))?:? (.*?)(?: \\(([\\w-]+)\\))?$"));

    QRegularExpressionMatch match;

    QVector<KDevelop::IProblem::Ptr> problems;

    foreach (const QString & line, lines) {
        match = problemRegex.match(line);
        if (match.hasMatch()) {
            const QString linter = match.captured(5);
            KDevelop::IProblem::Ptr problem(new KDevelop::DetectedProblem(
                linter.isEmpty() ? i18n("golangci-lint") : linter));
            //compilation problems are errors, everything else is a lint warning
            problem->setSeverity(linter == QLatin1String("typecheck") ? KDevelop::IProblem::Error
                                                                      : KDevelop::IProblem::Warning);
            problem->setDescription(match.captured(4));
            KDevelop::DocumentRange range;
            range.document = KDevelop::IndexedString(KDevelop::Path(m_projectRootPath, match.captured(1)).toLocalFile());
            range.setBothLines(match.captured(2).toInt() - 1);
            if(!match.captured(3).isEmpty())
            {
                range.setBothColumns(match.captured(3).toInt() - 1);
            }
            problem->setFinalLocation(range);
            problems.append(problem);
        }
    }

    emit problemsDetected(problems);

    if (status() == KDevelop::OutputExecuteJob::JobStatus::JobRunning) {
        KDevelop::OutputExecuteJob::postProcessStdout(lines);
    }
}

void Job::start()
{
    m_timer->restart();
    KDevelop::OutputExecuteJob::start();
}

void Job::childProcessError(QProcess::ProcessError e)
{
    QString message;

    switch (e) {
    case QProcess::FailedToStart:
        message = i18n("Failed to start golangci-lint from \"%1\".", commandLine()[0]);
        break;

    case QProcess::Crashed:
        if (status() != KDevelop::OutputExecuteJob::JobStatus::JobCanceled) {
            message = i18n("golangci-lint crashed.");
        }
        break;

    case QProcess::Timedout:
        message = i18n("golangci-lint process timed out.");
        break;

    case QProcess::WriteError:
        message = i18n("Write to golangci-lint process failed.");
        break;

    case QProcess::ReadError:
        message = i18n("Read from golangci-lint process failed.");
        break;

    case QProcess::UnknownError:
        break;
    }

    if (!message.isEmpty()) {
        KMessageBox::error(qApp->activeWindow(), message, i18n("golangci-lint Error"));
    }

    KDevelop::OutputExecuteJob::childProcessError(e);
}

}
