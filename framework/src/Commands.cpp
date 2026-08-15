// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/framework/Commands.h"

#include "lucida/core/diag/Log.h"

namespace lucida {

void CommandStack::Execute(std::unique_ptr<ICommand> command) {
    if (!command) return;
    command->Apply();
    Push(std::move(command));
}

void CommandStack::Push(std::unique_ptr<ICommand> command) {
    if (!command) return;

    // A new edit invalidates the redo branch. Keeping it would let the user redo
    // their way into a state that never existed.
    m_redo.clear();
    m_undo.push_back(std::move(command));

    // Bounded so a long session cannot grow without limit. The oldest entry goes,
    // which is the one least likely to be wanted.
    if (m_undo.size() > kMaxDepth) m_undo.erase(m_undo.begin());
}

bool CommandStack::Undo() {
    if (m_undo.empty()) return false;

    std::unique_ptr<ICommand> command = std::move(m_undo.back());
    m_undo.pop_back();
    command->Revert();
    LUCIDA_DEBUG(Framework, "undo: %s", command->Name());
    m_redo.push_back(std::move(command));
    return true;
}

bool CommandStack::Redo() {
    if (m_redo.empty()) return false;

    std::unique_ptr<ICommand> command = std::move(m_redo.back());
    m_redo.pop_back();
    command->Apply();
    LUCIDA_DEBUG(Framework, "redo: %s", command->Name());
    m_undo.push_back(std::move(command));
    return true;
}

void CommandStack::Clear() {
    m_undo.clear();
    m_redo.clear();
}

} // namespace lucida
