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

#ifndef BUILDJOB_H
#define BUILDJOB_H

#include <outputview/outputexecutejob.h>

/** Runs `go <arguments>` in the module root, with compiler-style error parsing. */
class GoBuildJob : public KDevelop::OutputExecuteJob
{
    Q_OBJECT
public:
    GoBuildJob(QObject* parent, const QStringList& arguments, const QUrl& moduleRoot);
    QStringList commandLine() const override;
private:
    QStringList m_arguments;
};
#endif // BUILDJOB_H
