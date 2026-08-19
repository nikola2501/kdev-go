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

#ifndef GOLANG_DLVVARIABLECONTROLLER_H
#define GOLANG_DLVVARIABLECONTROLLER_H

#include <debugger/interfaces/ivariablecontroller.h>
#include <debugger/variable/variablecollection.h>

namespace dlv {

class DebugSession;

class Variable : public KDevelop::Variable
{
    Q_OBJECT
public:
    Variable(DebugSession* session, KDevelop::TreeModel* model, KDevelop::TreeItem* parent,
             const QString& expression, const QString& display = {});

    void attachMaybe(QObject* callback = nullptr, const char* callbackMethod = nullptr) override;
    void fetchMoreChildren() override;

    /** Fill from a DAP variable object: value, type, children reference. */
    void updateFromDap(const QJsonObject& dapVariable);

private:
    DebugSession* m_session;
    int m_variablesReference = 0;
};

class VariableController : public KDevelop::IVariableController
{
    Q_OBJECT
public:
    explicit VariableController(DebugSession* session);

    KDevelop::Variable* createVariable(KDevelop::TreeModel* model, KDevelop::TreeItem* parent,
                                       const QString& expression, const QString& display = {}) override;
    KTextEditor::Range expressionRangeUnderCursor(KTextEditor::Document* doc,
                                                  const KTextEditor::Cursor& cursor) override;
    void addWatch(KDevelop::Variable* variable) override;
    void addWatchpoint(KDevelop::Variable* variable) override;

protected:
    void update() override;

private:
    void updateLocals();
    DebugSession* session() const;
};

}

#endif
