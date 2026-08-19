/* KDevelop go build support
 *
 * Copyright 2026 Nikola <nikolam2501@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <QDir>
#include <QFile>
#include <QObject>
#include <QTemporaryDir>
#include <QTest>

#include "utils.h"

using KDevelop::Path;

class GoUtilsTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void findsModuleRootFromNestedFolder();
    void fallsBackWithoutGoMod();
    void detectsMainPackage();
    void rejectsLibraryPackage();

private:
    QTemporaryDir m_tmp;
    QString m_module;
};

void GoUtilsTest::initTestCase()
{
    QVERIFY(m_tmp.isValid());
    m_module = m_tmp.path() + "/mod";
    QVERIFY(QDir().mkpath(m_module + "/cmd/tool"));
    QVERIFY(QDir().mkpath(m_module + "/internal/util"));

    auto write = [](const QString& path, const QByteArray& content) {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content);
    };
    write(m_module + "/go.mod", "module example.com/tool\n");
    write(m_module + "/cmd/tool/main.go", "// entry point\npackage main\n\nfunc main() {}\n");
    write(m_module + "/internal/util/util.go", "package util\n");
}

void GoUtilsTest::findsModuleRootFromNestedFolder()
{
    const Path fallback(m_tmp.path());
    QCOMPARE(Go::moduleRoot(Path(m_module + "/internal/util"), fallback), Path(m_module));
    QCOMPARE(Go::moduleRoot(Path(m_module), fallback), Path(m_module));
}

void GoUtilsTest::fallsBackWithoutGoMod()
{
    QVERIFY(QDir().mkpath(m_tmp.path() + "/nomod/sub"));
    const Path fallback(m_tmp.path() + "/nomod");
    QCOMPARE(Go::moduleRoot(Path(m_tmp.path() + "/nomod/sub"), fallback), fallback);
}

void GoUtilsTest::detectsMainPackage()
{
    QVERIFY(Go::isMainPackage(Path(m_module + "/cmd/tool")));
}

void GoUtilsTest::rejectsLibraryPackage()
{
    QVERIFY(!Go::isMainPackage(Path(m_module + "/internal/util")));
    QVERIFY(!Go::isMainPackage(Path(m_module)));
}

QTEST_GUILESS_MAIN(GoUtilsTest)

#include "test_goutils.moc"
