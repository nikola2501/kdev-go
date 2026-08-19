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

#include <QObject>
#include <QTemporaryDir>
#include <QTest>

#include "gomodule.h"

class GoModuleTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void findsModuleForNestedFile();
    void noModuleOutsideTree();
    void resolvesModuleLocalImport();
    void resolvesVendoredImport();
    void resolvesModuleCacheImport();
    void resolvesSubpackageOfRequiredModule();
    void escapesUppercaseInCachePaths();
    void resolvesLocalReplaceDirective();
    void resolvesUnlistedDependencyToNewestCachedVersion();
    void unknownImportResolvesToNothing();

private:
    void makeDir(const QString& path);
    void writeFile(const QString& path, const QByteArray& contents);

    QTemporaryDir m_tmp;
    QString m_proj;
    QString m_cache;
    QString m_mainFile;
};

void GoModuleTest::makeDir(const QString& path)
{
    QVERIFY(QDir().mkpath(path));
}

void GoModuleTest::writeFile(const QString& path, const QByteArray& contents)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(contents);
}

void GoModuleTest::initTestCase()
{
    QVERIFY(m_tmp.isValid());
    m_proj = m_tmp.path() + "/myapp";
    m_cache = m_tmp.path() + "/modcache";
    qputenv("GOMODCACHE", m_cache.toUtf8());

    makeDir(m_proj + "/internal/util");
    makeDir(m_proj + "/vendor/example.com/vendored");
    makeDir(m_proj + "/localdep");
    makeDir(m_cache + "/example.com/dep@v1.2.3/sub");
    makeDir(m_cache + "/github.com/!azure/thing@v0.5.0");
    makeDir(m_cache + "/example.com/unlisted@v0.1.0");
    makeDir(m_cache + "/example.com/unlisted@v0.2.0");

    writeFile(m_proj + "/go.mod",
              "module example.com/myapp\n"
              "\n"
              "go 1.22\n"
              "\n"
              "require (\n"
              "\texample.com/dep v1.2.3\n"
              "\tgithub.com/Azure/thing v0.5.0 // indirect\n"
              ")\n"
              "\n"
              "replace example.com/local => ./localdep\n");
    m_mainFile = m_proj + "/internal/util/util.go";
    writeFile(m_mainFile, "package util\n");
}

void GoModuleTest::findsModuleForNestedFile()
{
    go::GoModule module = go::GoModule::forFile(m_mainFile);
    QVERIFY(module.isValid());
    QCOMPARE(module.modulePath(), QString("example.com/myapp"));
    QCOMPARE(module.rootDir(), m_proj);
    QVERIFY(module.hasVendorDir());
}

void GoModuleTest::noModuleOutsideTree()
{
    go::GoModule module = go::GoModule::forFile("/nonexistent/path/file.go");
    QVERIFY(!module.isValid());
}

void GoModuleTest::resolvesModuleLocalImport()
{
    go::GoModule module = go::GoModule::forFile(m_mainFile);
    QCOMPARE(module.resolveImportDir("example.com/myapp/internal/util"),
             QString(m_proj + "/internal/util"));
}

void GoModuleTest::resolvesVendoredImport()
{
    go::GoModule module = go::GoModule::forFile(m_mainFile);
    QCOMPARE(module.resolveImportDir("example.com/vendored"),
             QString(m_proj + "/vendor/example.com/vendored"));
}

void GoModuleTest::resolvesModuleCacheImport()
{
    go::GoModule module = go::GoModule::forFile(m_mainFile);
    QCOMPARE(module.resolveImportDir("example.com/dep"),
             QString(m_cache + "/example.com/dep@v1.2.3"));
}

void GoModuleTest::resolvesSubpackageOfRequiredModule()
{
    go::GoModule module = go::GoModule::forFile(m_mainFile);
    QCOMPARE(module.resolveImportDir("example.com/dep/sub"),
             QString(m_cache + "/example.com/dep@v1.2.3/sub"));
}

void GoModuleTest::escapesUppercaseInCachePaths()
{
    QCOMPARE(go::GoModule::escapeModulePath("github.com/Azure/thing"),
             QString("github.com/!azure/thing"));
    go::GoModule module = go::GoModule::forFile(m_mainFile);
    QCOMPARE(module.resolveImportDir("github.com/Azure/thing"),
             QString(m_cache + "/github.com/!azure/thing@v0.5.0"));
}

void GoModuleTest::resolvesLocalReplaceDirective()
{
    go::GoModule module = go::GoModule::forFile(m_mainFile);
    QCOMPARE(module.resolveImportDir("example.com/local"),
             QString(m_proj + "/localdep"));
}

void GoModuleTest::resolvesUnlistedDependencyToNewestCachedVersion()
{
    go::GoModule module = go::GoModule::forFile(m_mainFile);
    QCOMPARE(module.resolveImportDir("example.com/unlisted"),
             QString(m_cache + "/example.com/unlisted@v0.2.0"));
}

void GoModuleTest::unknownImportResolvesToNothing()
{
    go::GoModule module = go::GoModule::forFile(m_mainFile);
    QVERIFY(module.resolveImportDir("example.com/never/heard/of/it").isEmpty());
}

QTEST_GUILESS_MAIN(GoModuleTest)

#include "gomoduletest.moc"
