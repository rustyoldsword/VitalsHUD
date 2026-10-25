#include "Typo.h"

#include "Settings.h"

#include <charconv>

namespace Vitals::Typo
{
	namespace
	{
		namespace DL = ImGuiMCP::ImDrawListManager;

		constexpr auto  kFontFolder = "Data/SKSE/Plugins/Fonts"sv;
		constexpr float kReference = 14.0f;  // weight and outline are given in pixels of 14 px text, the panel's stage names

		std::string Lower(std::string_view a_text)
		{
			std::string out(a_text);
			std::ranges::transform(out, out.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
			return out;
		}

		bool g_loaded = true;

		struct Outline
		{
			std::string hex;
			ImGuiMCP::ImU32 rgb{ 0 };   // alpha 0
			float           alpha{ 0.85f };
		};

		const Outline& OutlineColour()
		{
			static Outline cached{ "-", 0, 0.0f };
			const auto&    hex = Settings::Get().sOutlineColor;
			if (cached.hex != hex) {
				const auto c = ParseColour(hex);
				cached.hex = hex;
				cached.rgb = IM_COL32(static_cast<int>(c[0] * 255.0f), static_cast<int>(c[1] * 255.0f), static_cast<int>(c[2] * 255.0f), 0);
				cached.alpha = c[3];
			}
			return cached;
		}
	}

	ImGuiMCP::ImFont* Font()
	{
		auto*       base = ImGuiMCP::GetFont();
		const auto& name = Settings::Get().sFont;
		static std::string failed;
		if (name.empty()) {
			g_loaded = true;
			return base;
		}
		if (failed == name) {
			g_loaded = false;
			return base;
		}
		SKSEMenuFramework::PushFont(name);
		auto* font = ImGuiMCP::GetFont();
		FontAwesome::Pop();  // the framework's Pop clears its whole font stack
		g_loaded = font && font != base;
		if (!g_loaded) {
			failed = name;  // once per name: the framework warns in its own log on every miss
			logger::warn("Font '{}' is not loaded by SKSE Menu Framework (Data\\SKSE\\Plugins\\Fonts, read at game start); using the game font", name);
			return base;
		}
		failed.clear();
		return font;
	}

	bool Loaded()
	{
		return g_loaded;
	}

	void Draw(ImGuiMCP::ImDrawList* a_list, ImGuiMCP::ImFont* a_font, float a_size, ImGuiMCP::ImVec2 a_pos, ImGuiMCP::ImU32 a_col, const char* a_text, float a_wrap, float a_shadow)
	{
		const auto& s = Settings::Get();
		const float textAlpha = static_cast<float>((a_col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
		const float unit = a_size / kReference;
		const float weight = std::clamp(s.fTextWeight, 0.0f, 2.0f) * unit;
		const float outline = std::clamp(s.fOutline, 0.0f, 4.0f) * unit;
		// faux bold: the same text smeared to the right in half-pixel steps
		const int steps = weight > 0.05f ? static_cast<int>(std::ceil(weight / 0.5f)) + 1 : 1;
		auto      stepX = [&](int i) { return steps > 1 ? weight * static_cast<float>(i) / static_cast<float>(steps - 1) : 0.0f; };

		if (outline > 0.05f) {
			const auto&           o = OutlineColour();
			const ImGuiMCP::ImU32 col = o.rgb | (static_cast<ImGuiMCP::ImU32>(std::clamp(o.alpha * textAlpha, 0.0f, 1.0f) * 255.0f) << IM_COL32_A_SHIFT);
			// a ring of copies around both ends of the bold smear; a second, inner ring closes the gaps of a thick one
			const int ring = std::clamp(static_cast<int>(std::ceil(6.2832f * outline / 0.8f)), 8, 32);
			for (const float radius : { outline, outline * 0.5f }) {
				if (radius < outline && outline < 1.25f) {
					break;
				}
				const int n = radius < outline ? std::max(8, ring / 2) : ring;
				for (int e = 0; e < (steps > 1 ? 2 : 1); ++e) {
					const float ox = e == 0 ? 0.0f : weight;
					for (int i = 0; i < n; ++i) {
						const float angle = 6.2832f * static_cast<float>(i) / static_cast<float>(n);
						DL::AddText(a_list, a_font, a_size, { a_pos.x + ox + std::cos(angle) * radius, a_pos.y + std::sin(angle) * radius }, col, a_text, nullptr, a_wrap);
					}
				}
			}
		} else if (a_shadow > 0.0f) {
			const auto col = IM_COL32(0, 0, 0, static_cast<int>(std::clamp(a_shadow, 0.0f, 1.0f) * 255.0f));
			for (int i = 0; i < steps; ++i) {
				DL::AddText(a_list, a_font, a_size, { a_pos.x + 1.0f + stepX(i), a_pos.y + 1.0f }, col, a_text, nullptr, a_wrap);
			}
		}
		for (int i = 0; i < steps; ++i) {
			DL::AddText(a_list, a_font, a_size, { a_pos.x + stepX(i), a_pos.y }, a_col, a_text, nullptr, a_wrap);
		}
	}

	const std::vector<std::string>& Available(bool a_refresh)
	{
		static std::vector<std::string> names;
		static bool                     listed = false;
		if (listed && !a_refresh) {
			return names;
		}
		listed = true;
		names.clear();
		std::error_code ec;
		for (std::filesystem::directory_iterator it(std::filesystem::path(kFontFolder), ec), end; !ec && it != end; it.increment(ec)) {
			if (!it->is_regular_file(ec)) {
				continue;
			}
			const auto file = it->path().filename().string();
			const auto ext = Lower(it->path().extension().string());
			if ((ext == ".ttf" || ext == ".otf") && !Lower(file).starts_with("fa-")) {
				names.push_back(file);
			}
		}
		std::ranges::sort(names, [](const std::string& a, const std::string& b) { return Lower(a) < Lower(b); });
		return names;
	}

	std::array<float, 4> ParseColour(std::string_view a_hex)
	{
		std::array<float, 4> out{ 0.0f, 0.0f, 0.0f, 0.85f };
		if (!a_hex.empty() && a_hex.front() == '#') {
			a_hex.remove_prefix(1);
		}
		std::uint32_t value = 0;
		if ((a_hex.size() != 6 && a_hex.size() != 8) || std::from_chars(a_hex.data(), a_hex.data() + a_hex.size(), value, 16).ec != std::errc{}) {
			return out;
		}
		if (a_hex.size() == 6) {
			value = (value << 8) | 0xFF;
		}
		for (int i = 0; i < 4; ++i) {
			out[i] = static_cast<float>((value >> (24 - 8 * i)) & 0xFF) / 255.0f;
		}
		return out;
	}

	std::string FormatColour(const std::array<float, 4>& a_rgba)
	{
		auto byte = [](float v) { return static_cast<unsigned>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
		return std::format("{:02X}{:02X}{:02X}{:02X}", byte(a_rgba[0]), byte(a_rgba[1]), byte(a_rgba[2]), byte(a_rgba[3]));
	}
}
