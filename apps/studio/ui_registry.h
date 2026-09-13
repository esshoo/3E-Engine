#pragma once

#include "engine/data/json_value.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace threee::studio {

struct UiCommandItem {
    std::string type;
    std::string id;
    std::string label;
    std::string command;
};

struct UiMenu {
    std::string id;
    std::string label;
    std::string position;

    std::vector<UiCommandItem> items;
};

struct UiPanel {
    std::string id;
    std::string title;
    std::string dock;
    bool visible = true;
};

struct UiShortcut {
    std::string keys;
    std::string command;
};

struct UiLayoutSlot {
    std::string id;
    std::vector<std::string> panels;
};

class UiRegistry {
public:
    explicit UiRegistry(std::filesystem::path runtimeRoot);

    void SetActiveGame(std::string gameId);
    void Update(bool force = false);

    const std::vector<UiMenu>& GetMenus() const {
        return m_menus;
    }

    const std::vector<UiCommandItem>& GetToolbarItems() const {
        return m_toolbarItems;
    }

    const std::vector<UiPanel>& GetPanels() const {
        return m_panels;
    }

    const std::vector<UiShortcut>& GetShortcuts() const {
        return m_shortcuts;
    }

    const std::vector<UiLayoutSlot>& GetLayoutSlots() const {
        return m_layoutSlots;
    }

    const std::string& GetLastError() const {
        return m_lastError;
    }

    unsigned int GetGeneration() const {
        return m_generation;
    }

private:
    std::filesystem::path m_runtimeRoot;
    std::string m_activeGameId;

    std::vector<UiMenu> m_menus;
    std::vector<UiCommandItem> m_toolbarItems;
    std::vector<UiPanel> m_panels;
    std::vector<UiShortcut> m_shortcuts;
    std::vector<UiLayoutSlot> m_layoutSlots;

    std::chrono::steady_clock::time_point m_nextScan {};

    std::string m_lastError;
    std::string m_signature;

    unsigned int m_generation = 0;

    void Scan();
};

} // namespace threee::studio