#include "Diseases.h"

#include "DefaultDiseases.h"
#include "Settings.h"

#include <charconv>
#include <nlohmann/json.hpp>

// Ported from Disease Widget - SKSE 1.1 (kuroishitam, Nexus 195393; Manager.cpp / Settings.cpp), with the author's
// permission. Same definition format and merge rules, so disease files written for Disease Widget work unchanged.
namespace Vitals::Diseases
{
	namespace
	{
		using json = nlohmann::json;

		const std::filesystem::path kFolder = std::filesystem::path("Data") / "SKSE" / "Plugins" / "VitalsHUD";
		const std::filesystem::path kCore = kFolder / "Diseases.json";
		const std::array<std::filesystem::path, 2> kExtraFolders{ kFolder / "Diseases", std::filesystem::path("Data") / "SKSE" / "Plugins" / "DiseaseWidget" };

		std::mutex                            g_defsLock;
		std::shared_ptr<const DefinitionList> g_defs = std::make_shared<const DefinitionList>();

		std::mutex          g_lock;
		std::vector<Active> g_active;

		// main thread only
		std::unordered_map<RE::FormID, double> g_firstSeen;
		bool                                   g_firstScan{ true };

		std::string ToLower(std::string_view a_text)
		{
			std::string out(a_text);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		bool ParseColor(std::string_view a_text, std::array<float, 4>& a_out)
		{
			if (!a_text.empty() && a_text.front() == '#') {
				a_text.remove_prefix(1);
			}
			if (a_text.size() != 6 && a_text.size() != 8) {
				return false;
			}
			std::array<float, 4> parsed{ 0.0f, 0.0f, 0.0f, 1.0f };
			for (std::size_t i = 0; i < a_text.size() / 2; ++i) {
				const char*  first = a_text.data() + i * 2;
				unsigned int value = 0;
				const auto [ptr, ec] = std::from_chars(first, first + 2, value, 16);
				if (ec != std::errc{} || ptr != first + 2) {
					return false;
				}
				parsed[i] = static_cast<float>(value) / 255.0f;
			}
			a_out = parsed;
			return true;
		}

		bool ReadFile(const std::filesystem::path& a_path, json& a_out)
		{
			std::ifstream file(a_path);
			if (!file) {
				return false;
			}
			try {
				a_out = json::parse(file, nullptr, true, true);  // comments allowed
				return true;
			} catch (const std::exception& e) {
				logger::error("Could not parse {}: {}", a_path.string(), e.what());
				return false;
			}
		}

		void ResolveForms(Definition& a_def)
		{
			a_def.resolvedForms.clear();
			auto* data = RE::TESDataHandler::GetSingleton();
			if (!data) {
				return;
			}
			for (const auto& key : a_def.forms) {
				const auto bar = key.find('|');
				if (bar == std::string::npos || bar == 0 || bar + 1 >= key.size()) {
					logger::warn("Disease '{}': form '{}' is not 'Plugin.esp|0x1234'", a_def.id, key);
					continue;
				}
				RE::FormID local = 0;
				try {
					local = static_cast<RE::FormID>(std::stoul(key.substr(bar + 1), nullptr, 16));
				} catch (...) {
					logger::warn("Disease '{}': form '{}' is not 'Plugin.esp|0x1234'", a_def.id, key);
					continue;
				}
				if (const auto resolved = data->LookupFormID(local, key.substr(0, bar))) {
					a_def.resolvedForms.push_back(resolved);
				} else {
					logger::debug("Disease '{}': form '{}' not found (plugin not loaded?)", a_def.id, key);
				}
			}
		}

		void AppendUnique(std::vector<std::string>& a_list, std::string a_value)
		{
			if (std::ranges::find(a_list, a_value) == a_list.end()) {
				a_list.push_back(std::move(a_value));
			}
		}

		// An id that already exists is merged: match names and forms are added, name / icon / color / requireDiseaseType
		// change only when the entry has them.
		void ApplyEntry(DefinitionList& a_list, const json& a_item, const std::string& a_source)
		{
			const std::string id = a_item.value("id", std::string{});
			if (id.empty()) {
				logger::warn("{}: skipped an entry without an 'id'", a_source);
				return;
			}
			auto it = std::ranges::find(a_list, id, &Definition::id);
			if (it == a_list.end()) {
				Definition fresh;
				fresh.id = id;
				fresh.displayName = id;
				fresh.icon = id;
				fresh.source = a_source;
				a_list.push_back(std::move(fresh));
				it = std::prev(a_list.end());
			} else if (it->source.find(a_source) == std::string::npos) {
				it->source += ", " + a_source;
			}
			auto& def = *it;
			if (a_item.contains("name") && a_item["name"].is_string()) {
				def.displayName = a_item["name"].get<std::string>();
			}
			if (a_item.contains("icon") && a_item["icon"].is_string()) {
				def.icon = a_item["icon"].get<std::string>();
			}
			if (a_item.contains("requireDiseaseType") && a_item["requireDiseaseType"].is_boolean()) {
				def.requireDiseaseType = a_item["requireDiseaseType"].get<bool>();
			}
			if (a_item.contains("match") && a_item["match"].is_array()) {
				for (const auto& n : a_item["match"]) {
					if (n.is_string()) {
						AppendUnique(def.names, ToLower(n.get<std::string>()));
					}
				}
			}
			if (a_item.contains("forms") && a_item["forms"].is_array()) {
				for (const auto& f : a_item["forms"]) {
					if (f.is_string()) {
						AppendUnique(def.forms, f.get<std::string>());
					}
				}
			}
			if (a_item.contains("color") && a_item["color"].is_string()) {
				if (ParseColor(a_item["color"].get<std::string>(), def.tint)) {
					def.hasTint = true;
				} else {
					logger::warn("{}: disease '{}' has an invalid colour", a_source, id);
				}
			}
		}

		std::size_t ApplyFile(DefinitionList& a_list, const json& a_root, const std::string& a_source)
		{
			const auto it = a_root.find("diseases");
			if (it == a_root.end() || !it->is_array()) {
				return 0;  // not a definition file
			}
			std::size_t count = 0;
			for (const auto& item : *it) {
				try {
					ApplyEntry(a_list, item, a_source);
					++count;
				} catch (const std::exception& e) {
					logger::warn("{}: skipped a malformed entry: {}", a_source, e.what());
				}
			}
			return count;
		}

		// our Diseases.json first, then every *.json below VitalsHUD/Diseases and DiseaseWidget, alphabetically per folder
		std::vector<std::filesystem::path> CollectFiles()
		{
			std::vector<std::filesystem::path> files{ kCore };
			for (const auto& folder : kExtraFolders) {
				std::vector<std::filesystem::path> found;
				std::error_code                    ec;
				if (!std::filesystem::exists(folder, ec)) {
					continue;
				}
				for (std::filesystem::recursive_directory_iterator dir(folder, std::filesystem::directory_options::skip_permission_denied, ec), end; !ec && dir != end; dir.increment(ec)) {
					if (!dir->is_regular_file(ec) || ToLower(dir->path().extension().string()) != ".json") {
						continue;
					}
					if (ToLower(dir->path().filename().string()) == "settings.json") {
						continue;  // Disease Widget's own settings
					}
					found.push_back(dir->path());
				}
				std::ranges::sort(found, [](const auto& a, const auto& b) { return ToLower(a.generic_string()) < ToLower(b.generic_string()); });
				files.insert(files.end(), found.begin(), found.end());
			}
			return files;
		}

		std::string SourceName(const std::filesystem::path& a_path)
		{
			std::error_code ec;
			const auto      relative = std::filesystem::relative(a_path, std::filesystem::path("Data") / "SKSE" / "Plugins", ec);
			return ec ? a_path.filename().string() : relative.generic_string();
		}

		const Definition* Find(const DefinitionList& a_defs, const RE::SpellItem* a_spell, bool a_isDisease)
		{
			const RE::FormID  id = a_spell->GetFormID();
			const char*       raw = a_spell->GetName();
			const std::string name = raw ? ToLower(raw) : std::string{};
			// exact forms win, even for spells that are not of type Disease (Survival Mode ailments)
			for (const auto& def : a_defs) {
				if (std::ranges::find(def.resolvedForms, id) != def.resolvedForms.end()) {
					return &def;
				}
			}
			for (const auto& def : a_defs) {
				if (def.requireDiseaseType && !a_isDisease) {
					continue;
				}
				if (!name.empty() && std::ranges::find(def.names, name) != def.names.end()) {
					return &def;
				}
			}
			return nullptr;
		}

		std::vector<std::string> HiddenIds()
		{
			std::vector<std::string> ids;
			std::string_view         text = Settings::Get().sDiseaseHide;
			while (!text.empty()) {
				const auto comma = text.find(',');
				auto       part = text.substr(0, comma);
				while (!part.empty() && part.front() == ' ') part.remove_prefix(1);
				while (!part.empty() && part.back() == ' ') part.remove_suffix(1);
				if (!part.empty()) {
					ids.emplace_back(part);
				}
				if (comma == std::string_view::npos) {
					break;
				}
				text.remove_prefix(comma + 1);
			}
			return ids;
		}
	}

	double NowSeconds()
	{
		using Clock = std::chrono::steady_clock;
		static const auto start = Clock::now();
		return std::chrono::duration<double>(Clock::now() - start).count();
	}

	void LoadDefinitions()
	{
		std::error_code ec;
		if (!std::filesystem::exists(kCore, ec)) {
			std::filesystem::create_directories(kFolder, ec);
			if (std::ofstream file(kCore, std::ios::trunc); file) {
				file << kDefaultDiseasesJson;
				logger::info("Wrote the default {}", kCore.string());
			}
		}
		auto        list = std::make_shared<DefinitionList>();
		std::size_t files = 0;
		for (const auto& path : CollectFiles()) {
			json root;
			if (!ReadFile(path, root)) {
				if (path != kCore) {
					continue;
				}
				logger::warn("Using the built-in disease definitions");
				root = json::parse(kDefaultDiseasesJson);
			}
			if (const auto read = ApplyFile(*list, root, SourceName(path)); read > 0) {
				++files;
				logger::info("{}: {} disease entries", SourceName(path), read);
			}
		}
		for (auto& def : *list) {
			ResolveForms(def);
		}
		logger::info("Diseases: {} definitions from {} file(s)", list->size(), files);
		std::scoped_lock lock(g_defsLock);
		g_defs = std::move(list);
	}

	std::shared_ptr<const DefinitionList> Definitions()
	{
		std::scoped_lock lock(g_defsLock);
		return g_defs;
	}

	void Sample()
	{
		if (Settings::Get().iDiseases == 2) {
			std::scoped_lock lock(g_lock);
			g_active.clear();
			return;
		}
		const double                   now = NowSeconds();
		const auto                     defs = Definitions();
		std::vector<Active>            found;
		std::unordered_set<RE::FormID> seen;

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* target = player ? player->AsMagicTarget() : nullptr;
		// main thread; the list is not locked, so only plain data is copied out of each effect
		if (auto* effects = target ? target->GetActiveEffectList() : nullptr) {
			for (auto* effect : *effects) {
				if (!effect || !effect->spell) {
					continue;
				}
				const auto* spell = effect->spell->As<RE::SpellItem>();
				if (!spell || !seen.insert(spell->GetFormID()).second) {
					continue;
				}
				const bool isDisease = spell->GetSpellType() == RE::MagicSystem::SpellType::kDisease;
				const auto* def = Find(*defs, spell, isDisease);
				if (!def && !isDisease) {
					continue;
				}
				Active      entry;
				const char* raw = spell->GetName();
				const bool  named = raw && *raw;
				if (def) {
					entry.id = def->id;
					entry.name = def->displayName;
					entry.stage = named ? raw : def->displayName;
					entry.icon = def->icon;
					entry.tint = def->tint;
					entry.hasTint = def->hasTint;
				} else {
					entry.id = std::format("form:{:08X}", spell->GetFormID());
					entry.name = named ? raw : "Unknown disease";
					entry.stage = entry.name;
					entry.icon = "generic";
					entry.known = false;
				}
				const auto [it, inserted] = g_firstSeen.try_emplace(spell->GetFormID(), now);
				if (inserted && g_firstScan) {
					it->second = now - 3600.0;  // already there when the save loaded: no "new" pulse
				}
				entry.firstSeen = it->second;
				found.push_back(std::move(entry));
			}
		}
		std::erase_if(g_firstSeen, [&](const auto& a_kv) { return !seen.contains(a_kv.first); });
		g_firstScan = false;

		// several spells of one disease (an infection and its stage) show as one icon labelled with the longest spell name
		std::unordered_map<std::string, std::size_t> byKey;
		std::vector<Active>                          merged;
		for (auto& e : found) {
			if (!e.known) {
				merged.push_back(std::move(e));
				continue;
			}
			const std::string key = e.icon + "|" + ToLower(e.name);
			if (const auto it = byKey.find(key); it == byKey.end()) {
				byKey.emplace(key, merged.size());
				merged.push_back(std::move(e));
			} else {
				auto& m = merged[it->second];
				if (e.stage.size() > m.stage.size()) {
					m.stage = e.stage;
				}
				m.firstSeen = std::min(m.firstSeen, e.firstSeen);
			}
		}
		for (auto& m : merged) {
			if (ToLower(m.stage) == ToLower(m.name)) {
				m.stage.clear();
			}
		}
		std::ranges::sort(merged, [](const Active& a, const Active& b) { return a.firstSeen != b.firstSeen ? a.firstSeen < b.firstSeen : a.name < b.name; });

		std::scoped_lock lock(g_lock);
		g_active = std::move(merged);
	}

	void OnGameLoaded()
	{
		SKSE::GetTaskInterface()->AddTask([] {
			g_firstSeen.clear();
			g_firstScan = true;
		});
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		g_active.clear();
	}

	std::vector<Active> ReadAll()
	{
		std::scoped_lock lock(g_lock);
		return g_active;
	}

	std::vector<Active> Read()
	{
		auto        all = ReadAll();
		const auto  hidden = HiddenIds();
		const bool  unknown = Settings::Get().bDiseaseUnknown;
		std::erase_if(all, [&](const Active& a) { return (!a.known && !unknown) || std::ranges::find(hidden, a.id) != hidden.end(); });
		return all;
	}

	bool IsHidden(const std::string& a_id)
	{
		const auto hidden = HiddenIds();
		return std::ranges::find(hidden, a_id) != hidden.end();
	}

	void SetHidden(const std::string& a_id, bool a_hidden)
	{
		auto hidden = HiddenIds();
		std::erase(hidden, a_id);
		if (a_hidden) {
			hidden.push_back(a_id);
		}
		std::ranges::sort(hidden);
		std::string text;
		for (const auto& id : hidden) {
			text += (text.empty() ? "" : ",") + id;
		}
		Settings::Get().sDiseaseHide = std::move(text);
	}

	std::string Describe()
	{
		const auto defs = Definitions();
		std::size_t resolved = 0;
		for (const auto& def : *defs) {
			resolved += def.resolvedForms.size();
		}
		return std::format("{} definitions, {} forms found", defs->size(), resolved);
	}
}
