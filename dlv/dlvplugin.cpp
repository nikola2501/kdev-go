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

#include "dlvplugin.h"

#include "dlvdebug.h"
#include "dlvlauncher.h"

#include <execute/iexecuteplugin.h>
#include <interfaces/icore.h>
#include <interfaces/iplugincontroller.h>
#include <interfaces/iruncontroller.h>
#include <interfaces/launchconfigurationtype.h>

#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(DlvFactory, "kdevdlv.json", registerPlugin<dlv::Plugin>();)

namespace dlv {

Plugin::Plugin(QObject* parent, const KPluginMetaData& metaData, const QVariantList&)
    : IPlugin(QStringLiteral("kdevdlv"), parent, metaData)
{
    //attach the Delve launcher to every native-application launch
    //configuration type so "Debug" can use delve for Go binaries
    const auto plugins = core()->pluginController()->allPluginsForExtension(QStringLiteral("org.kdevelop.IExecutePlugin"));
    for(auto* plugin : plugins)
    {
        auto* iface = plugin->extension<IExecutePlugin>();
        Q_ASSERT(iface);
        auto* type = core()->runController()->launchConfigurationTypeForId(iface->nativeAppConfigTypeId());
        Q_ASSERT(type);
        type->addLauncher(new Launcher(iface));
    }
}

void Plugin::unload()
{
}

}

#include "dlvplugin.moc"
