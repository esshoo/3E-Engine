#include "apps/studio/command_registry.h"

#include <algorithm>
#include <sstream>
#include <system_error>
#include <unordered_map>
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

std::vector<std::filesystem::path> CollectJsonFiles(
    const std::filesystem::path& directory) {

    std::vector<std::filesystem::path> files;
    std::error_code ec;

    if (!std::filesystem::exists(directory, ec)) {
        return files;
    }

    std::filesystem::directory_iterator iterator(
        directory,
        ec);

    if (ec) {
        return files;
    }

    for (const auto& entry : iterator) {
        if (!entry.is_regular_file(ec)) {
            ec.clear();
            continue;
        }

        const auto& path = entry.path();

        if (path.extension() == ".json") {
            files.push_back(path);
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

void LoadCommandFile(
    const std::filesystem::path& file,
    std::unordered_map<std::string, CommandDescriptor>& commands,
    std::string& error) {

    const data::JsonDocument document =
        data::JsonDocument::LoadFile(file);

    if (!document.Ok()) {
        if (!error.empty()) {
            error += "\n";
        }

        error +=
            "Cannot parse command file " +
            file.string() +
            ": " +
            document.error;

        return;
    }

    if (!document.root.IsObject()) {
        return;
    }

    const data::JsonValue* commandArray =
        document.root.Find("commands");

    if (!commandArray ||
        !commandArray->IsArray()) {
        return;
    }

    for (const data::JsonValue& value :
         commandArray->arrayValue) {

        if (!value.IsObject()) {
            continue;
        }

        CommandDescriptor command;
        command.id = value.GetString("id");
        command.sourceFile = file;

        const data::JsonValue* action =
            value.Find("action");

        if (action && action->IsObject()) {
            command.action = *action;
            command.actionType =
                action->GetString("type");
            command.handler =
                action->GetString("handler");
        }

        if (!command.IsValid()) {
            continue;
        }

        // Later sources override earlier ones. This allows a game
        // to override a shared command without changing the Host.
        commands.insert_or_assign(
            command.id,
            std::move(command));
    }
}

} // namespace

CommandRegistry::CommandRegistry(
    std::filesystem::path runtimeRoot)
    : m_runtimeRoot(std::move(runtimeRoot)) {

    Update(true);
}

void CommandRegistry::SetActiveGame(
    std::string gameId) {

    if (m_activeGameId == gameId) {
        return;
    }

    m_activeGameId = std::move(gameId);
    Update(true);
}

void CommandRegistry::Update(bool force) {
    const auto now =
        std::chrono::steady_clock::now();

    if (!force && now < m_nextScan) {
        return;
    }

    m_nextScan =
        now + std::chrono::milliseconds(250);

    Scan();
}

const CommandDescriptor* CommandRegistry::Find(
    const std::string& commandId) const {

    const auto it = m_index.find(commandId);

    if (it == m_index.end() ||
        it->second >= m_commands.size()) {
        return nullptr;
    }

    return &m_commands[it->second];
}

void CommandRegistry::Scan() {
    const std::filesystem::path coreDirectory =
        m_runtimeRoot /
        "config" /
        "commands";

    std::vector<std::filesystem::path> sources =
        CollectJsonFiles(coreDirectory);

    if (!m_activeGameId.empty()) {
        const std::filesystem::path gameDirectory =
            m_runtimeRoot /
            "games" /
            m_activeGameId /
            "commands";

        const auto gameSources =
            CollectJsonFiles(gameDirectory);

        sources.insert(
            sources.end(),
            gameSources.begin(),
            gameSources.end());
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

    std::unordered_map<std::string, CommandDescriptor> map;
    std::string error;

    for (const auto& source : sources) {
        LoadCommandFile(
            source,
            map,
            error);
    }

    std::vector<CommandDescriptor> commands;
    commands.reserve(map.size());

    for (auto& [id, command] : map) {
        commands.push_back(std::move(command));
    }

    std::sort(
        commands.begin(),
        commands.end(),
        [](const CommandDescriptor& a,
           const CommandDescriptor& b) {
            return a.id < b.id;
        });

    std::unordered_map<std::string, std::size_t> index;

    for (std::size_t i = 0;
         i < commands.size();
         ++i) {

        index.emplace(
            commands[i].id,
            i);
    }

    m_signature = signatureText;
    m_commands = std::move(commands);
    m_index = std::move(index);
    m_lastError = std::move(error);

    ++m_generation;
}

} // namespace threee::studio