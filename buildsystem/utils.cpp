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

#include "utils.h"

#include <project/projectmodel.h>

#include <QDir>
#include <QFile>
#include <QRegularExpression>

using namespace KDevelop;

namespace Go
{

Path moduleRoot(const Path& path, const Path& fallback)
{
    QDir dir(path.toLocalFile());
    while(dir.exists())
    {
        if(dir.exists(QStringLiteral("go.mod")))
            return Path(dir.absolutePath());
        if(!dir.cdUp())
            break;
    }
    return fallback;
}

Path moduleRoot(ProjectBaseItem* item)
{
    const Path projectPath = item->project()->path();
    return moduleRoot(item->path(), projectPath);
}

QString packagePattern(ProjectBaseItem* item, bool recursive)
{
    const Path root = moduleRoot(item);
    Path folderPath = item->path();
    if(item->file())
        folderPath = folderPath.parent();
    QString relative = root.relativePath(folderPath);
    QString pattern = QStringLiteral("./");
    if(!relative.isEmpty() && relative != QLatin1String("."))
        pattern += relative;
    if(recursive)
        pattern += pattern.endsWith(QLatin1Char('/')) ? QStringLiteral("...") : QStringLiteral("/...");
    else if(pattern == QLatin1String("./"))
        pattern = QStringLiteral(".");
    return pattern;
}

bool isMainPackage(const Path& folder)
{
    static const QRegularExpression packageMain(QStringLiteral("^\\s*package\\s+main\\b"));
    QDir dir(folder.toLocalFile());
    const auto goFiles = dir.entryList(QStringList(QStringLiteral("*.go")), QDir::Files | QDir::NoSymLinks);
    for(const QString& file : goFiles)
    {
        QFile f(dir.filePath(file));
        if(!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        //the package clause is near the top; scanning a few lines is enough
        for(int line = 0; line < 50 && !f.atEnd(); ++line)
        {
            if(packageMain.match(QString::fromUtf8(f.readLine())).hasMatch())
                return true;
        }
    }
    return false;
}

}
