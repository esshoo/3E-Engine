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

bool LoadMenus(
    const std::filesystem::path& file,
    std::vector<UiMenu>& outMenus,
    std::string& error) {

    if (!std::filesystem::exists(file)) {
        return true;
    }

    const data::JsonDocument document =
        data::JsonDocument::LoadFile(file);

    if (!document.Ok()) {
        error =
            "Cannot parse UI file " +
            file.string() +
            ": " +
            document.error;

        return false;
    }

    if (!document.root.IsObject()) {
        return true;
    }

    const data::JsonValue* menus =
        document.root.Find("menus");

    if (!menus || !menus->IsArray()) {
        return true;
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

    return true;
}

bool LoadToolbar(
    const std::filesystem::path& file,
    std::vector<UiCommandItem>& outItems,
    std::string& error) {

    if (!std::filesystem::exists(file)) {
        return true;
    }

    const data::JsonDocument document =
        data::JsonDocument::LoadFile(file);

    if (!document.Ok()) {
        error =
            "Cannot parse UI file " +
            file.string() +
            ": " +
            document.error;

        return false;
    }

    if (!document.root.IsObject()) {
        return true;
    }

    const data::JsonValue* items =
        document.root.Find("items");

    if (!items || !items->IsArray()) {
        return true;
    }

    for (const data::JsonValue& value :
         items->arrayValue) {

        outItems.push_back(
            ParseItem(value));
    }

    return true;
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
    const std::filesystem::path defaultMenu =
        m_runtimeRoot /
        "config" /
        "ui" /
        "default" /
        "main_menu.json";

    const std::filesystem::path defaultToolbar =
        m_runtimeRoot /
        "config" /
        "ui" /
        "default" /
        "toolbar.json";

    std::filesystem::path gameMenu;
    std::filesystem::path gameToolbar;

    if (!m_activeGameId.empty()) {
        gameMenu =
            m_runtimeRoot /
            "games" /
            m_activeGameId /
            "ui" /
            "main_menu.json";

        gameToolbar =
            m_runtimeRoot /
            "games" /
            m_activeGameId /
            "ui" /
            "toolbar.json";
    }

    std::ostringstream signature;

    signature
        << "game="
        << m_activeGameId
        << '\n'
        << defaultMenu.string()
        << '|'
        << FileStamp(defaultMenu)
        << '\n'
        << defaultToolbar.string()
        << '|'
        << FileStamp(defaultToolbar)
        << '\n';

    if (!gameMenu.empty()) {
        signature
            << gameMenu.string()
            << '|'
            << FileStamp(gameMenu)
            << '\n'
            << gameToolbar.string()
            << '|'
            << FileStamp(gameToolbar)
            << '\n';
    }

    const std::string signatureText =
        signature.str();

    if (signatureText == m_signature) {
        return;
    }

    std::vector<UiMenu> menus;
    std::vector<UiCommandItem> toolbar;
    std::string error;

    LoadMenus(
        defaultMenu,
        menus,
        error);

    LoadToolbar(
        defaultToolbar,
        toolbar,
        error);

    if (!gameMenu.empty()) {
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
    }

    m_signature =
        signatureText;

    m_menus =
        std::move(menus);

    m_toolbarItems =
        std::move(toolbar);

    m_lastError =
        std::move(error);

    ++m_generation;
}

} // namespace threee::studio