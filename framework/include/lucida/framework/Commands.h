// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Undo and redo as a command stack (GPP ch.2: Command).
//
// The rule that makes it work, and that editors get wrong: an edit is one
// command from the user's point of view, not one per frame. Dragging a slider
// for two seconds is a single undo entry - captured when the control is grabbed,
// pushed when it is released. Anything that writes a component directly, without
// going through here, does not exist as far as undo is concerned.

#include "lucida/core/ecs/Registry.h"

#include <memory>
#include <string>
#include <vector>

namespace lucida {

class ICommand {
public:
    virtual ~ICommand() = default;

    virtual void Apply() = 0;
    virtual void Revert() = 0;
    virtual const char* Name() const = 0;
};

class CommandStack {
public:
    // Applies the command and takes ownership. Anything previously undone is
    // discarded: branching histories are a research project, not an editor.
    void Execute(std::unique_ptr<ICommand> command);

    // Pushes an already-applied change. For controls that edited the value live
    // and only now know what it was before.
    void Push(std::unique_ptr<ICommand> command);

    bool Undo();
    bool Redo();

    bool CanUndo() const { return !m_undo.empty(); }
    bool CanRedo() const { return !m_redo.empty(); }
    const char* UndoName() const { return m_undo.empty() ? "" : m_undo.back()->Name(); }
    const char* RedoName() const { return m_redo.empty() ? "" : m_redo.back()->Name(); }

    void Clear();
    usize Depth() const { return m_undo.size(); }

private:
    static constexpr usize kMaxDepth = 256;

    std::vector<std::unique_ptr<ICommand>> m_undo;
    std::vector<std::unique_ptr<ICommand>> m_redo;
};

// Moving, rotating or scaling an entity. Stores both states rather than a delta:
// a delta has to be re-derived on every redo and drifts when floats round.
class TransformCommand final : public ICommand {
public:
    TransformCommand(Registry& registry, Entity entity,
                     const LocalTransform& before, const LocalTransform& after,
                     std::string name = "Transform")
        : m_registry(registry), m_entity(entity), m_before(before), m_after(after),
          m_name(std::move(name)) {}

    void Apply() override  { Write(m_after); }
    void Revert() override { Write(m_before); }
    const char* Name() const override { return m_name.c_str(); }

private:
    void Write(const LocalTransform& value) {
        if (LocalTransform* local = m_registry.Get<LocalTransform>(m_entity)) *local = value;
    }

    Registry&      m_registry;
    Entity         m_entity;
    LocalTransform m_before;
    LocalTransform m_after;
    std::string    m_name;
};

} // namespace lucida
