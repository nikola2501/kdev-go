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

#ifndef GOLANGMODULE_H
#define GOLANGMODULE_H

#include <QHash>
#include <QString>

#include "kdevgoparser_export.h"

namespace go {

/**
 * Go modules (go.mod) support: resolves import paths of a module-based
 * project to directories on disk.
 *
 * Resolution order matches the go tool:
 *  1. imports within the module itself (module path prefix)
 *  2. vendor/ directory, when the module is vendored
 *  3. replace directives (local paths and module@version)
 *  4. the module cache ($GOMODCACHE, default $GOPATH/pkg/mod) using the
 *     versions pinned in the require directives; when an indirect
 *     dependency is not listed, the newest cached version is used
 */
class KDEVGOPARSER_EXPORT GoModule
{
public:
    /**
     * Find and parse the go.mod governing @p filePath by walking up the
     * directory tree. Results are cached per go.mod (invalidated on
     * modification), so calling this from every parse job is cheap.
     */
    static GoModule forFile(const QString& filePath);

    bool isValid() const { return !m_modulePath.isEmpty(); }
    QString modulePath() const { return m_modulePath; }
    QString rootDir() const { return m_rootDir; }
    bool hasVendorDir() const { return m_hasVendor; }

    /**
     * @return the directory containing the sources of @p importPath,
     *         or an empty string when the import cannot be resolved.
     */
    QString resolveImportDir(const QString& importPath) const;

    /** Escape a module path or version for the module cache layout:
     *  uppercase letters become '!' + lowercase ("github.com/Azure" -> "github.com/!azure"). */
    static QString escapeModulePath(const QString& path);

    /** The module cache root: $GOMODCACHE, `go env GOMODCACHE` or $HOME/go/pkg/mod. */
    static QString moduleCacheDir();

private:
    void parse(const QString& goModFile);
    QString resolveInModuleCache(const QString& modulePath, const QString& version, const QString& rest) const;

    QString m_modulePath;
    QString m_rootDir;
    QHash<QString, QString> m_requires; //module path -> version
    struct Replacement { QString target; QString version; }; //local dir when version is empty and target is a path
    QHash<QString, Replacement> m_replaces;
    bool m_hasVendor = false;
};

}

#endif
