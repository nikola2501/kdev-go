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

#include "builder.h"

#include "buildjob.h"
#include "buildsystem.h"
#include "executabletargetitem.h"
#include "utils.h"

#include <project/projectmodel.h>

KJob* GoBuilder::build( KDevelop::ProjectBaseItem *item )
{
    //building a specific executable target compiles that single main
    //package, which makes `go` write the binary into the module root;
    //everything else is a recursive compile check without artifacts
    if(auto* target = dynamic_cast<GoExecutableTargetItem*>(item))
        return createJobForAction(target->parent(), QStringLiteral("build"), false);
    return createJobForAction(item, QStringLiteral("build"), true);
}

KJob* GoBuilder::clean( KDevelop::ProjectBaseItem *item )
{
    return createJobForAction(item, QStringLiteral("clean"), true);
}

KJob* GoBuilder::install(KDevelop::ProjectBaseItem *item, const QUrl &installPath)
{
    Q_UNUSED(installPath)
    return createJobForAction(item, QStringLiteral("install"), true);
}

KJob *GoBuilder::createJobForAction(KDevelop::ProjectBaseItem *item, const QString &action, bool recursive) const
{
    auto moduleRoot = Go::moduleRoot(item);
    QStringList arguments{action, Go::packagePattern(item, recursive)};
    return new GoBuildJob(nullptr, arguments, moduleRoot.toUrl());
}
