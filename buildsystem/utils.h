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

#ifndef UTILS_H
#define UTILS_H

#include "gobuildsystem_export.h"

#include <util/path.h>
#include <interfaces/iproject.h>

namespace KDevelop {
    class ProjectBaseItem;
}

namespace Go
{
    /** The nearest directory at or above @p path containing go.mod, or @p fallback. */
    KDEVGOBUILDSYSTEM_EXPORT KDevelop::Path moduleRoot(const KDevelop::Path& path, const KDevelop::Path& fallback);

    /** Module root for a project item (project root when there is no go.mod). */
    KDEVGOBUILDSYSTEM_EXPORT KDevelop::Path moduleRoot(KDevelop::ProjectBaseItem* item);

    /** Package pattern for the go tool relative to the module root:
     *  "./..." for the module root itself, "./internal/ui/..." for a subfolder. */
    KDEVGOBUILDSYSTEM_EXPORT QString packagePattern(KDevelop::ProjectBaseItem* item, bool recursive = true);

    /** True when the folder contains a "package main" .go file. */
    KDEVGOBUILDSYSTEM_EXPORT bool isMainPackage(const KDevelop::Path& folder);
}

#endif // UTILS_H
