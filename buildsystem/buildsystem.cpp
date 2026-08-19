/* KDevelop go build support
 *
 * Copyright 2017 Mikhail Ivchenko <ematirov@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "buildsystem.h"

#include "builder.h"
#include "executabletargetitem.h"
#include "utils.h"
#include "buildjob.h"

#include <interfaces/icore.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/iplugincontroller.h>
#include <kpluginfactory.h>
#include <project/helper.h>
#include <interfaces/iruncontroller.h>

#include <KActionCollection>
#include <KLocalizedString>

#include <QAction>
#include <QIcon>

using namespace KDevelop;

K_PLUGIN_FACTORY_WITH_JSON(BuildSystemFactory, "buildsystem.json", registerPlugin<GoBuildSystem>(); )

GoBuildSystem::GoBuildSystem(QObject* parent, const KPluginMetaData& metaData, const QVariantList& args)
: KDevelop::AbstractFileManagerPlugin(QStringLiteral("gobuildsystem"), parent, metaData), m_builder(new GoBuilder())
{
    Q_UNUSED(args)
    setXMLFile( "buildsystem.rc" );

    auto* testAction = new QAction(i18n("Run Go Tests"), this);
    testAction->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
    connect(testAction, &QAction::triggered, this, &GoBuildSystem::runTests);
    actionCollection()->addAction(QStringLiteral("go_test_all"), testAction);
}

void GoBuildSystem::runTests()
{
    //`go test ./...` for every open project that is a Go module
    const auto projects = core()->projectController()->projects();
    for(auto* project : projects)
    {
        const auto root = Go::moduleRoot(project->path(), Path());
        if(root.isEmpty())
            continue;
        auto* job = new GoBuildJob(this, {QStringLiteral("test"), QStringLiteral("./...")}, root.toUrl());
        core()->runController()->registerJob(job);
    }
}

GoBuildSystem::~GoBuildSystem()
{
}

IProjectBuilder* GoBuildSystem::builder() const
{
    return m_builder;
}

Path::List GoBuildSystem::includeDirectories(KDevelop::ProjectBaseItem*) const
{
    return {};
}

Path::List GoBuildSystem::frameworkDirectories(KDevelop::ProjectBaseItem*) const
{
    return {};
}

QHash<QString,QString> GoBuildSystem::defines(KDevelop::ProjectBaseItem*) const
{
    return {};
}

QString GoBuildSystem::extraArguments(KDevelop::ProjectBaseItem*) const
{
    return {};
}

ProjectTargetItem* GoBuildSystem::createTarget(const QString& target, KDevelop::ProjectFolderItem *parent)
{
    Q_UNUSED(target)
    Q_UNUSED(parent)
    return nullptr;
}

bool GoBuildSystem::addFilesToTarget(const QList< ProjectFileItem* > &files, ProjectTargetItem* parent)
{
    Q_UNUSED( files )
    Q_UNUSED( parent )
    return false;
}

bool GoBuildSystem::removeTarget(KDevelop::ProjectTargetItem *target)
{
    Q_UNUSED( target )
    return false;
}

bool GoBuildSystem::removeFilesFromTargets(const QList< ProjectFileItem* > &targetFiles)
{
    Q_UNUSED( targetFiles )
    return false;
}

bool GoBuildSystem::hasBuildInfo(KDevelop::ProjectBaseItem* item) const
{
    Q_UNUSED(item);
    return false;
}

Path GoBuildSystem::compiler(KDevelop::ProjectTargetItem* p) const
{
    Q_UNUSED(p);
    return Path("go");
}

Path GoBuildSystem::buildDirectory(KDevelop::ProjectBaseItem* item) const
{
    //the go tool operates on the module: everything builds from its root
    return Go::moduleRoot(item);
}

QList<ProjectTargetItem*> GoBuildSystem::targets(KDevelop::ProjectFolderItem*) const
{
    return {};
}

KDevelop::ProjectFolderItem * GoBuildSystem::createFolderItem(KDevelop::IProject* project, const KDevelop::Path& path, KDevelop::ProjectBaseItem* parent)
{
    ProjectBuildFolderItem *item = new KDevelop::ProjectBuildFolderItem(project, path, parent);
    //only a "package main" directory produces an executable
    if(Go::isMainPackage(path))
        new GoExecutableTargetItem(item, item->folderName());

    return item;
}



#include "buildsystem.moc"
