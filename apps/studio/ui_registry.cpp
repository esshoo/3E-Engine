#include "apps/studio/ui_registry.h"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <system_error>
#include <utility>

namespace threee::studio {

namespace {

std::uint64_t FileStamp(
    const std::filesystem::path& path) {

    std::error_code ec;

    const auto time =
        std::filesystem::last_write_time(
            path,
            ec);

    if (ec) {
        return 0;
    }

    return static_cast<std::uint64_t>(
        time.time_since_epoch().count());
}

void AppendError(
    std::string& error,
    const std::string& message) {

    if (!error.empty()) {
        error += "\n";
    }

    error += message;
}

UiCommandItem ParseItem(
    const data::JsonValue& value) {

    UiCommandItem item;

    if (!value.IsObject()) {
        return item;
    }

    item.type =
        value.GetString("type");

    item.id =
        value.GetString("id");

    item.label =
        value.GetString("label");

    item.command =
        value.GetString("command");

    return item;
}

bool LoadDocument(
    const std::filesystem::path& file,
    data::JsonDocument& document,
    std::string& error) {

    std::error_code ec;

    if (!std::filesystem::exists(file, ec)) {
        return false;
    }

    document =
        data::JsonDocument::LoadFile(file);

    if (!document.Ok()) {
        AppendError(
            error,
            "Cannot parse UI file " +
            file.string() +
            ": " +
            document.error);

        return false;
    }

    return true;
}

void LoadMenus(
    const std::filesystem::path& file,
    std::vector<UiMenu>& outMenus,
    std::string& error) {

    data::JsonDocument document;

    if (!LoadDocument(
            file,
            document,
            error)) {
        return;
    }

    if (!document.root.IsObject()) {
        return;
    }

    const data::JsonValue* menus =
        document.root.Find("menus");

    if (!menus || !menus->IsArray()) {
        return;
    }

    for (const data::JsonValue& value :
         menus->arrayValue) {

        if (!value.IsObject()) {
            continue;
        }

        UiMenu menu;

        menu.id =
            value.GetString("id");

        menu.label =
            value.GetString(
                "label",
                menu.id);

        menu.position =
            value.GetString("position");

        const data::JsonValue* items =
            value.Find("items");

        if (items && items->IsArray()) {
            for (const data::JsonValue& itemValue :
                 items->arrayValue) {

                menu.items.push_back(
                    ParseItem(itemValue));
            }
        }

        if (!menu.id.empty()) {
            outMenus.push_back(
                std::move(menu));
        }
    }
}

void LoadToolbar(
    const std::filesystem::path& file,
    std::vector<UiCommandItem>& outItems,
    std::string& error) {

    data::JsonDocument document;

    if (!LoadDocument(
            file,
            document,
            error)) {
        return;
    }

    if (!document.root.IsObject()) {
        return;
    }

    const data::JsonValue* items =
        document.root.Find("items");

    if (!items || !items->IsArray()) {
        return;
    }

    for (const data::JsonValue& value :
         items->arrayValue) {

        outItems.push_back(
            ParseItem(value));
    }
}

void LoadPanels(
    const std::filesystem::path& file,
    std::vector<UiPanel>& outPanels,
    std::string& error) {

    data::JsonDocument document;

    if (!LoadDocument(
            file,
            document,
            error)) {
        return;
    }

    if (!document.root.IsObject()) {
        return;
    }

    const data::JsonValue* panels =
        document.root.Find("panels");

    if (!panels || !panels->IsArray()) {
        return;
    }

    for (const data::JsonValue& value :
         panels->arrayValue) {

        if (!value.IsObject()) {
            continue;
        }

        UiPanel panel;

        panel.id =
            value.GetString("id");

        panel.title =
            value.GetString(
                "title",
                panel.id);

        panel.dock =
            value.GetString("dock");

        panel.visible =
            value.GetBool(
                "visible",
                true);

        if (!panel.id.empty()) {
            outPanels.push_back(
                std::move(panel));
        }
    }
}

void LoadShortcuts(
    const std::filesystem::path& file,
    std::vector<UiShortcut>& outShortcuts,
    std::string& error) {

    data::JsonDocument document;

    if (!LoadDocument(
            file,
            document,
            error)) {
        return;
    }

    if (!document.root.IsObject()) {
        return;
    }

    const data::JsonValue* bindings =
        document.root.Find("bindings");

    if (!bindings || !bindings->IsArray()) {
        return;
    }

    for (const data::JsonValue& value :
         bindings->arrayValue) {

        if (!value.IsObject()) {
            continue;
        }

        UiShortcut shortcut;

        shortcut.keys =
            value.GetString("keys");

        shortcut.command =
            value.GetString("command");

        if (!shortcut.keys.empty() &&
            !shortcut.command.empty()) {

            outShortcuts.push_back(
                std::move(shortcut));
        }
    }
}

void LoadLayout(
    const std::filesystem::path& file,
    std::vector<UiLayoutSlot>& outSlots,
    std::string& error) {

    data::JsonDocument document;

    if (!LoadDocument(
            file,
            document,
            error)) {
        return;
    }

    if (!document.root.IsObject()) {
        return;
    }

    const data::JsonValue* dockspace =
        document.root.Find("dockspace");

    if (!dockspace ||
        !dockspace->IsObject()) {
        return;
    }

    for (const auto& [slotId, value] :
         dockspace->objectValue) {

        if (!value.IsArray()) {
            continue;
        }

        UiLayoutSlot slot;
        slot.id = slotId;

        for (const data::JsonValue& panel :
             value.arrayValue) {

            if (panel.IsString()) {
                slot.panels.push_back(
                    panel.stringValue);
            }
        }

        outSlots.push_back(
            std::move(slot));
    }

    std::sort(
        outSlots.begin(),
        outSlots.end(),
        [](const UiLayoutSlot& a,
           const UiLayoutSlot& b) {
            return a.id < b.id;
        });
}

void MergeMenu(
    std::vector<UiMenu>& menus,
    UiMenu incoming) {

    const auto existing =
        std::find_if(
            menus.begin(),
            menus.end(),
            [&incoming](const UiMenu& menu) {
                return menu.id == incoming.id;
            });

    if (existing != menus.end()) {
        *existing = std::move(incoming);
        return;
    }

    const std::string beforePrefix = "before:";
    const std::string afterPrefix = "after:";

    if (incoming.position.rfind(
            beforePrefix,
            0) == 0) {

        const std::string target =
            incoming.position.substr(
                beforePrefix.size());

        const auto targetIt =
            std::find_if(
                menus.begin(),
                menus.end(),
                [&target](const UiMenu& menu) {
                    return menu.id == target;
                });

        if (targetIt != menus.end()) {
            menus.insert(
                targetIt,
                std::move(incoming));
            return;
        }
    }

    if (incoming.position.rfind(
            afterPrefix,
            0) == 0) {

        const std::string target =
            incoming.position.substr(
                afterPrefix.size());

        const auto targetIt =
            std::find_if(
                menus.begin(),
                menus.end(),
                [&target](const UiMenu& menu) {
                    return menu.id == target;
                });

        if (targetIt != menus.end()) {
            menus.insert(
                targetIt + 1,
                std::move(incoming));
            return;
        }
    }

    menus.push_back(
        std::move(incoming));
}

void MergeToolbarItem(
    std::vector<UiCommandItem>& items,
    UiCommandItem incoming) {

    if (!incoming.id.empty()) {
        const auto existing =
            std::find_if(
                items.begin(),
                items.end(),
                [&incoming](const UiCommandItem& item) {
                    return item.id == incoming.id;
                });

        if (existing != items.end()) {
            *existing = std::move(incoming);
            return;
        }
    }

    items.push_back(
        std::move(incoming));
}

void MergePanel(
    std::vector<UiPanel>& panels,
    UiPanel incoming) {

    const auto existing =
        std::find_if(
            panels.begin(),
            panels.end(),
            [&incoming](const UiPanel& panel) {
                return panel.id == incoming.id;
            });

    if (existing != panels.end()) {
        *existing = std::move(incoming);
        return;
    }

    panels.push_back(
        std::move(incoming));
}

void MergeShortcut(
    std::vector<UiShortcut>& shortcuts,
    UiShortcut incoming) {

    const auto existing =
        std::find_if(
            shortcuts.begin(),
            shortcuts.end(),
            [&incoming](const UiShortcut& shortcut) {
                return shortcut.keys == incoming.keys;
            });

    if (existing != shortcuts.end()) {
        *existing = std::move(incoming);
        return;
    }

    shortcuts.push_back(
        std::move(incoming));
}

void MergeLayoutSlot(
    std::vector<UiLayoutSlot>& slots,
    UiLayoutSlot incoming) {

    const auto existing =
        std::find_if(
            slots.begin(),
            slots.end(),
            [&incoming](const UiLayoutSlot& slot) {
                return slot.id == incoming.id;
            });

    if (existing != slots.end()) {
        *existing = std::move(incoming);
        return;
    }

    slots.push_back(
        std::move(incoming));
}

void ApplyLayoutToPanels(
    std::vector<UiPanel>& panels,
    const std::vector<UiLayoutSlot>& slots) {

    for (const UiLayoutSlot& slot :
         slots) {

        for (const std::string& panelId :
             slot.panels) {

            const auto panel =
                std::find_if(
                    panels.begin(),
                    panels.end(),
                    [&panelId](const UiPanel& value) {
                        return value.id == panelId;
                    });

            if (panel != panels.end()) {
                panel->dock = slot.id;
            }
        }
    }
}

} // namespace

UiRegistry::UiRegistry(
    std::filesystem::path runtimeRoot)
    : m_runtimeRoot(std::move(runtimeRoot)) {

    Update(true);
}

void UiRegistry::SetActiveGame(
    std::string gameId) {

    if (m_activeGameId == gameId) {
        return;
    }

    m_activeGameId =
        std::move(gameId);

    Update(true);
}

void UiRegistry::Update(bool force) {
    const auto now =
        std::chrono::steady_clock::now();

    if (!force && now < m_nextScan) {
        return;
    }

    m_nextScan =
        now + std::chrono::milliseconds(250);

    Scan();
}

void UiRegistry::Scan() {
    const std::filesystem::path defaultRoot =
        m_runtimeRoot /
        "config" /
        "ui" /
        "default";

    const std::filesystem::path defaultMenu =
        defaultRoot / "main_menu.json";

    const std::filesystem::path defaultToolbar =
        defaultRoot / "toolbar.json";

    const std::filesystem::path defaultPanels =
        defaultRoot / "panels.json";

    const std::filesystem::path defaultShortcuts =
        defaultRoot / "shortcuts.json";

    const std::filesystem::path defaultLayout =
        defaultRoot / "layout.json";

    std::filesystem::path gameRoot;
    std::filesystem::path gameMenu;
    std::filesystem::path gameToolbar;
    std::filesystem::path gamePanels;
    std::filesystem::path gameShortcuts;
    std::filesystem::path gameLayout;

    if (!m_activeGameId.empty()) {
        gameRoot =
            m_runtimeRoot /
            "games" /
            m_activeGameId /
            "ui";

        gameMenu =
            gameRoot / "main_menu.json";

        gameToolbar =
            gameRoot / "toolbar.json";

        gamePanels =
            gameRoot / "panels.json";

        gameShortcuts =
            gameRoot / "shortcuts.json";

        gameLayout =
            gameRoot / "layout.json";
    }

    std::vector<std::filesystem::path> sources {
        defaultMenu,
        defaultToolbar,
        defaultPanels,
        defaultShortcuts,
        defaultLayout
    };

    if (!gameRoot.empty()) {
        sources.push_back(gameMenu);
        sources.push_back(gameToolbar);
        sources.push_back(gamePanels);
        sources.push_back(gameShortcuts);
        sources.push_back(gameLayout);
    }

    std::ostringstream signature;

    signature
        << "game="
        << m_activeGameId
        << '\n';

    for (const auto& source : sources) {
        signature
            << source.string()
            << '|'
            << FileStamp(source)
            << '\n';
    }

    const std::string signatureText =
        signature.str();

    if (signatureText == m_signature) {
        return;
    }

    std::vector<UiMenu> menus;
    std::vector<UiCommandItem> toolbar;
    std::vector<UiPanel> panels;
    std::vector<UiShortcut> shortcuts;
    std::vector<UiLayoutSlot> layoutSlots;

    std::string error;

    LoadMenus(
        defaultMenu,
        menus,
        error);

    LoadToolbar(
        defaultToolbar,
        toolbar,
        error);

    LoadPanels(
        defaultPanels,
        panels,
        error);

    LoadShortcuts(
        defaultShortcuts,
        shortcuts,
        error);

    LoadLayout(
        defaultLayout,
        layoutSlots,
        error);

    if (!gameRoot.empty()) {
        std::vector<UiMenu> gameMenus;
        LoadMenus(
            gameMenu,
            gameMenus,
            error);

        for (UiMenu& menu : gameMenus) {
            MergeMenu(
                menus,
                std::move(menu));
        }

        std::vector<UiCommandItem> gameToolbarItems;
        LoadToolbar(
            gameToolbar,
            gameToolbarItems,
            error);

        for (UiCommandItem& item :
             gameToolbarItems) {

            MergeToolbarItem(
                toolbar,
                std::move(item));
        }

        std::vector<UiPanel> gamePanelItems;
        LoadPanels(
            gamePanels,
            gamePanelItems,
            error);

        for (UiPanel& panel :
             gamePanelItems) {

            MergePanel(
                panels,
                std::move(panel));
        }

        std::vector<UiShortcut> gameShortcutItems;
        LoadShortcuts(
            gameShortcuts,
            gameShortcutItems,
            error);

        for (UiShortcut& shortcut :
             gameShortcutItems) {

            MergeShortcut(
                shortcuts,
                std::move(shortcut));
        }

        std::vector<UiLayoutSlot> gameLayoutSlots;
        LoadLayout(
            gameLayout,
            gameLayoutSlots,
            error);

        for (UiLayoutSlot& slot :
             gameLayoutSlots) {

            MergeLayoutSlot(
                layoutSlots,
                std::move(slot));
        }
    }

    ApplyLayoutToPanels(
        panels,
        layoutSlots);

    m_signature =
        signatureText;

    m_menus =
        std::move(menus);

    m_toolbarItems =
        std::move(toolbar);

    m_panels =
        std::move(panels);

    m_shortcuts =
        std::move(shortcuts);

    m_layoutSlots =
        std::move(layoutSlots);

    m_lastError =
        std::move(error);

    ++m_generation;
}

} // namespace threee::studio