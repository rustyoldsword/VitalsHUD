#pragma once

#pragma warning(push)
#pragma warning(disable : 4099 5054)
#include <SKSEMenuFramework.h>
#pragma warning(pop)

namespace Vitals::Typo
{
	// the text font for this frame: [Text] sFont when SKSE Menu Framework loaded it, else the framework's own font
	ImGuiMCP::ImFont* Font();

	// text with the [Text] weight and outline; a_shadow is the old 1 px drop shadow, used only while the outline is off
	void Draw(ImGuiMCP::ImDrawList* a_list, ImGuiMCP::ImFont* a_font, float a_size, ImGuiMCP::ImVec2 a_pos, ImGuiMCP::ImU32 a_col, const char* a_text, float a_wrap = 0.0f,
		float a_shadow = 0.0f);

	// font files in Data/SKSE/Plugins/Fonts without the FontAwesome icon fonts, sorted; a_refresh lists the folder again
	const std::vector<std::string>& Available(bool a_refresh = false);

	// false when sFont is set but the framework has no such font (missing file, or added after the game started)
	bool Loaded();

	// RRGGBBAA <-> 0..1 floats
	std::array<float, 4> ParseColour(std::string_view a_hex);
	std::string          FormatColour(const std::array<float, 4>& a_rgba);
}
