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

#include "gomodule.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QStandardPaths>

namespace go {

namespace {

struct CacheEntry {
    QDateTime modified;
    GoModule module;
};

QMutex cacheMutex;
QHash<QString, CacheEntry> moduleCache; //keyed by absolute go.mod path
QString cachedModCacheDir;

QString goEnv(const QString& name)
{
    QProcess p;
    p.start("go", {"env", name});
    p.waitForFinished();
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

}

GoModule GoModule::forFile(const QString& filePath)
{
    QDir dir = QFileInfo(filePath).absoluteDir();
    QString goModFile;
    while(dir.exists())
    {
        if(dir.exists("go.mod"))
        {
            goModFile = dir.absoluteFilePath("go.mod");
            break;
        }
        if(!dir.cdUp())
            break;
    }
    if(goModFile.isEmpty())
        return GoModule();

    QDateTime modified = QFileInfo(goModFile).lastModified();
    QMutexLocker lock(&cacheMutex);
    auto cached = moduleCache.constFind(goModFile);
    if(cached != moduleCache.constEnd() && cached->modified == modified)
        return cached->module;

    GoModule module;
    module.parse(goModFile);
    moduleCache.insert(goModFile, {modified, module});
    return module;
}

void GoModule::parse(const QString& goModFile)
{
    QFile file(goModFile);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    m_rootDir = QFileInfo(goModFile).absolutePath();
    m_hasVendor = QDir(m_rootDir + "/vendor").exists();

    auto parseDirective = [this](const QString& directive, const QString& line) {
        const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if(directive == "require" && parts.size() >= 2)
            m_requires.insert(parts.at(0), parts.at(1));
        else if(directive == "replace")
        {
            //`old [version] => new [version]`
            int arrow = parts.indexOf("=>");
            if(arrow >= 1 && arrow + 1 < parts.size())
            {
                Replacement replacement;
                replacement.target = parts.at(arrow + 1);
                if(arrow + 2 < parts.size())
                    replacement.version = parts.at(arrow + 2);
                m_replaces.insert(parts.at(0), replacement);
            }
        }
    };

    QString block; //current parenthesized block: "require", "replace" or "exclude"
    while(!file.atEnd())
    {
        QString line = QString::fromUtf8(file.readLine());
        int comment = line.indexOf("//");
        if(comment != -1)
            line.truncate(comment);
        line = line.trimmed();
        if(line.isEmpty())
            continue;

        if(!block.isEmpty())
        {
            if(line == ")")
                block.clear();
            else
                parseDirective(block, line);
            continue;
        }

        const QStringList words = line.split(' ', Qt::SkipEmptyParts);
        if(words.size() == 2 && words.last() == "(" &&
           (words.first() == "require" || words.first() == "replace" || words.first() == "exclude"))
        {
            block = words.first();
        }
        else if(words.first() == "module" && words.size() >= 2)
        {
            m_modulePath = words.at(1);
        }
        else if(words.size() >= 3 && (words.first() == "require" || words.first() == "replace"))
        {
            parseDirective(words.first(), line.mid(words.first().length()).trimmed());
        }
        //anything else: go directive, exclude, retract, toolchain...
    }
}

QString GoModule::escapeModulePath(const QString& path)
{
    QString escaped;
    escaped.reserve(path.length());
    for(const QChar c : path)
    {
        if(c.isUpper())
        {
            escaped += '!';
            escaped += c.toLower();
        }
        else
            escaped += c;
    }
    return escaped;
}

QString GoModule::moduleCacheDir()
{
    QMutexLocker lock(&cacheMutex);
    if(!cachedModCacheDir.isNull())
        return cachedModCacheDir;
    lock.unlock();

    QString dir = QString::fromUtf8(qgetenv("GOMODCACHE"));
    if(dir.isEmpty())
        dir = goEnv("GOMODCACHE");
    if(dir.isEmpty())
        dir = QDir::homePath() + "/go/pkg/mod";

    lock.relock();
    cachedModCacheDir = dir;
    return cachedModCacheDir;
}

QString GoModule::resolveInModuleCache(const QString& modulePath, const QString& version, const QString& rest) const
{
    QDir cache(moduleCacheDir());
    if(!cache.exists())
        return QString();

    const QString escaped = escapeModulePath(modulePath);
    if(!version.isEmpty())
    {
        QString candidate = cache.absolutePath() + '/' + escaped + '@' + escapeModulePath(version) + rest;
        if(QDir(candidate).exists())
            return candidate;
    }
    //version not pinned in go.mod (e.g. an indirect dependency in an old
    //go.mod): fall back to the newest version present in the cache
    int slash = escaped.lastIndexOf('/');
    QDir parent(cache.absolutePath() + '/' + (slash == -1 ? QString() : escaped.left(slash)));
    if(!parent.exists())
        return QString();
    const QString base = slash == -1 ? escaped : escaped.mid(slash + 1);
    QStringList versions = parent.entryList(QStringList(base + "@*"), QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    if(versions.isEmpty())
        return QString();
    QString candidate = parent.absoluteFilePath(versions.last()) + rest;
    return QDir(candidate).exists() ? candidate : QString();
}

QString GoModule::resolveImportDir(const QString& importPath) const
{
    if(!isValid())
        return QString();

    //1. the import points into this module itself
    if(importPath == m_modulePath)
        return m_rootDir;
    if(importPath.startsWith(m_modulePath + '/'))
    {
        QString candidate = m_rootDir + '/' + importPath.mid(m_modulePath.length() + 1);
        if(QDir(candidate).exists())
            return candidate;
        return QString();
    }

    //2. vendored dependencies: the vendor tree mirrors import paths exactly
    if(m_hasVendor)
    {
        QString candidate = m_rootDir + "/vendor/" + importPath;
        if(QDir(candidate).exists())
            return candidate;
        //when a module is vendored the go tool never falls back to the
        //module cache, but being more forgiving costs nothing here
    }

    //3. replace directives and the module cache, matching the longest
    //module-path prefix of the import path
    QString prefix = importPath;
    while(true)
    {
        auto replace = m_replaces.constFind(prefix);
        const QString rest = importPath.mid(prefix.length());
        if(replace != m_replaces.constEnd())
        {
            const QString& target = replace->target;
            if(target.startsWith("./") || target.startsWith("../") || target.startsWith('/'))
            {
                QString candidate = QDir::cleanPath(m_rootDir + '/' + target) + rest;
                if(QDir(candidate).exists())
                    return candidate;
            }
            else
            {
                QString candidate = resolveInModuleCache(target, replace->version, rest);
                if(!candidate.isEmpty())
                    return candidate;
            }
        }
        auto require = m_requires.constFind(prefix);
        if(require != m_requires.constEnd())
        {
            QString candidate = resolveInModuleCache(prefix, require.value(), rest);
            if(!candidate.isEmpty())
                return candidate;
        }
        int slash = prefix.lastIndexOf('/');
        if(slash == -1)
            break;
        prefix.truncate(slash);
    }

    //4. an import that is not in go.mod at all: try the newest cached version
    //working from the longest prefix down, so subpackages still resolve
    prefix = importPath;
    while(true)
    {
        QString candidate = resolveInModuleCache(prefix, QString(), importPath.mid(prefix.length()));
        if(!candidate.isEmpty())
            return candidate;
        int slash = prefix.lastIndexOf('/');
        if(slash == -1)
            break;
        prefix.truncate(slash);
    }
    return QString();
}

}
