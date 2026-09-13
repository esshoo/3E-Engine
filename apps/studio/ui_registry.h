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

    std::chrono::steady_clock::time_point m_nextScan {};

    std::string m_lastError;
    std::string m_signature;

    unsigned int m_generation = 0;

    void Scan();
};

} // namespace threee::studio