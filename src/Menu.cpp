#include "Menu.h"

#include "Diseases.h"
#include "Feed.h"
#include "Hud.h"
#include "Meters.h"
#include "Settings.h"
#include "Sls.h"
#include "Typo.h"

#pragma warning(push)
#pragma warning(disable : 4099 5054)
#include <SKSEMenuFramework.h>
#pragma warning(pop)

namespace Vitals::Menu
{
	namespace
	{
		void Help(const char* a_text)
		{
			if (a_text && ImGuiMCP::IsItemHovered()) {
				ImGuiMCP::SetTooltip("%s", a_text);
			}
		}

		void Toggle(const char* a_label, bool& a_value, const char* a_help = nullptr)
		{
			if (ImGuiMCP::Checkbox(a_label, std::addressof(a_value))) {
				Settings::Save();
			}
			Help(a_help);
		}

		void Slider(const char* a_label, float& a_value, float a_min, float a_max, const char* a_format, const char* a_help = nullptr)
		{
			if (ImGuiMCP::SliderFloat(a_label, std::addressof(a_value), a_min, a_max, a_format)) {
				Settings::Save();
			}
			Help(a_help);
		}

		void __stdcall Panel()
		{
			auto& s = Settings::Get();
			ImGuiMCP::SeparatorText("Panel");
			Toggle("Show the panel", s.bEnabled);
			bool preview = Hud::PreviewEnabled();
			if (ImGuiMCP::Checkbox("Preview while this menu is open", std::addressof(preview))) {
				Hud::SetPreview(preview);
			}
			Help("Shows every meter (sample values for mods you do not have or meters at rest). Drag the panel with the mouse to move it.");
			Toggle("Compact panel", s.bCompact, "Only the stage pictures and trend arrows. The expand key opens the detailed panel with bars and stage names.");
			{
				static constexpr std::array<std::pair<int, const char*>, 11> kKeys{ { { 0, "None" }, { 60, "F2" }, { 61, "F3" }, { 62, "F4" }, { 63, "F5" }, { 64, "F6" }, { 65, "F7" },
					{ 66, "F8" }, { 67, "F9" }, { 68, "F10" }, { 88, "F12" } } };
				int current = 0;
				for (int i = 0; i < static_cast<int>(kKeys.size()); ++i) {
					if (kKeys[i].first == s.iExpandKey) {
						current = i;
					}
				}
				std::array<const char*, kKeys.size()> names{};
				for (std::size_t i = 0; i < kKeys.size(); ++i) {
					names[i] = kKeys[i].second;
				}
				ImGuiMCP::SetNextItemWidth(110.0f);
				if (ImGuiMCP::Combo("Expand key", std::addressof(current), names.data(), static_cast<int>(names.size()))) {
					s.iExpandKey = kKeys[current].first;
					Settings::Save();
				}
				Help("Switches between the compact and the detailed panel while playing.");
				ImGuiMCP::SameLine();
				ImGuiMCP::TextDisabled("%s", Hud::Expanded() ? "(detailed now)" : "(compact now)");
			}
			Toggle("Horizontal layout", s.bHorizontal);
			Toggle("Anchor at the right edge", s.bAlignRight, "The position is the panel's right edge, so it grows to the left.");
			Toggle("Stage names", s.bShowLabels, "\"Chilly\", \"Peckish\"... above each bar. Off: bars only.");
			Toggle("Values", s.bShowValues, "Percentages next to the stage name.");
			Slider("Position X", s.fPosX, 0.0f, 1.0f, "%.3f");
			Slider("Position Y", s.fPosY, 0.0f, 1.0f, "%.3f");
			Slider("Scale", s.fScale, 0.5f, 2.5f, "%.2f");
			Slider("Bar length", s.fBarWidth, 60.0f, 320.0f, "%.0f px");
			Slider("Picture size", s.fIconSize, 1.0f, 2.5f, "%.2fx", "Size of the stage pictures and the SL Survival status pictures, relative to the text.");
			Slider("Opacity", s.fOpacity, 0.1f, 1.0f, "%.2f");
			Slider("Background", s.fBackground, 0.0f, 1.0f, "%.2f", "The dark panel behind the pictures and bars. 0: none (the preview then shows only a thin frame to drag).");
			Slider("Icon plates", s.fIconPlate, 0.0f, 1.0f, "%.2f", "The dark disc under the disease icons (and under letters when a picture is missing). 0: icons without a plate.");

			ImGuiMCP::SeparatorText("Trend arrows");
			Slider("Sensitivity", s.fSensitivity, 0.25f, 4.0f, "%.2fx", "Higher shows arrows for slower changes. One arrow: slow, two: fast, three: very fast or a sudden jump.");
			ImGuiMCP::TextWrapped("Arrows are green when the change is good for you (warming up, getting clean) and red when it is bad. Wetness and arousal have no good direction and use their own colour.");

			ImGuiMCP::SeparatorText("Hide");
			Toggle("In combat", s.bHideInCombat);
			Toggle("During SexLab scenes", s.bHideInScenes);
			Toggle("In dialogue", s.bHideInDialogue);
			Slider("Keep a settled meter for", s.fLinger, 0.0f, 60.0f, "%.0f s", "Auto meters stay this long after they return to rest and stop changing.");

			ImGuiMCP::SeparatorText("Maintenance");
			if (ImGuiMCP::Button("Reload INI")) {
				Settings::Load();
			}
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button("Reset defaults")) {
				Settings::Reset();
			}
			ImGuiMCP::TextWrapped("Settings: Data\\SKSE\\Plugins\\VitalsHUD.ini");
		}

		void __stdcall Meters()
		{
			static constexpr const char* kModes[]{ "Auto", "Always", "Never" };
			ImGuiMCP::TextWrapped("Auto shows a meter only while it matters: away from its resting stage or changing.");
			ImGuiMCP::Spacing();
			for (std::size_t i = 0; i < kMeterCount; ++i) {
				const auto id = static_cast<MeterId>(i);
				const auto& info = Info(id);
				auto&       mode = Visibility(id);
				ImGuiMCP::PushID(static_cast<int>(i));
				ImGuiMCP::SetNextItemWidth(110.0f);
				if (ImGuiMCP::Combo(info.title, std::addressof(mode), kModes, 3)) {
					Settings::Save();
				}
				ImGuiMCP::SameLine();
				ImGuiMCP::TextDisabled("%s", Vitals::Meters::Describe(id).c_str());
				ImGuiMCP::PopID();
			}
			{
				auto& mode = Settings::Get().iSls;
				ImGuiMCP::SetNextItemWidth(110.0f);
				if (ImGuiMCP::Combo("SL Survival", std::addressof(mode), kModes, 3)) {
					Settings::Save();
				}
				Help("Licences, toll, curfew, curses and orgasm fatigue under the meters. Compact: short tags, and only the licences you hold; the expand key shows them with names and also lists the licences you lack. Always also shows missing licences, curses and fatigue at rest in the compact panel.");
				ImGuiMCP::SameLine();
				ImGuiMCP::TextDisabled("%s", Sls::Describe().c_str());
			}
			{
				auto& mode = Settings::Get().iDiseases;
				ImGuiMCP::SetNextItemWidth(110.0f);
				if (ImGuiMCP::Combo("Diseases", std::addressof(mode), kModes, 3)) {
					Settings::Save();
				}
				Help("One icon per disease you carry, under the meters. Auto: only while you are ill. Always: also \"No diseases\". More options on the Diseases page.");
				ImGuiMCP::SameLine();
				ImGuiMCP::TextDisabled("%s", Diseases::Describe().c_str());
			}
			ImGuiMCP::Spacing();
			ImGuiMCP::SeparatorText("At rest (hidden in Auto)");
			ImGuiMCP::BulletText("Wetness: dry. Arousal: below 20. Dirt: clean.");
			ImGuiMCP::BulletText("Warmth, hunger, fatigue: the first two Survival Mode stages.");
			ImGuiMCP::BulletText("Cycle: follicular and luteal phases (ovulation, menstruation and pregnancy show).");
			ImGuiMCP::BulletText("Cum craving: satisfied, and only once SL Survival's addiction has started.");
		}

		void __stdcall DiseasesPage()
		{
			static constexpr const char* kModes[]{ "Auto", "Always", "Never" };
			auto& s = Settings::Get();
			ImGuiMCP::TextWrapped("Every disease on your character shows as a round icon under the meters; the expand key adds the stage (\"Mild Ataxia\"). Disease detection, definitions and icons come from Disease Widget - SKSE by kuroishitam.");
			ImGuiMCP::Spacing();
			ImGuiMCP::SetNextItemWidth(110.0f);
			if (ImGuiMCP::Combo("Show", std::addressof(s.iDiseases), kModes, 3)) {
				Settings::Save();
			}
			Help("Auto: only while you are ill. Always: also \"No diseases\". Never: the block is off and nothing is scanned.");
			Toggle("Colour by disease", s.bDiseaseColors, "Each disease tints its icon with its own colour from the definition file. Off: all icons in parchment white.");
			Toggle("Unknown diseases", s.bDiseaseUnknown, "Spells of type Disease that no definition file names get the generic icon. Off: they are not shown.");
			Toggle("Pulse new diseases", s.bDiseasePulse, "The ring of a newly caught disease pulses for 5 seconds.");

			ImGuiMCP::SeparatorText("Now");
			const auto active = Diseases::ReadAll();
			if (active.empty()) {
				ImGuiMCP::TextDisabled("No diseases.");
			}
			for (const auto& d : active) {
				ImGuiMCP::BulletText("%s%s%s%s", d.name.c_str(), d.stage.empty() ? "" : " - ", d.stage.c_str(), d.known ? "" : " (no definition, generic icon)");
			}

			ImGuiMCP::SeparatorText("Diseases");
			ImGuiMCP::TextWrapped("Untick to hide a disease. Definitions: Data\\SKSE\\Plugins\\VitalsHUD\\Diseases.json, then every .json in VitalsHUD\\Diseases and DiseaseWidget (the same format as Disease Widget, so its add-on files work).");
			if (ImGuiMCP::Button("Reload definitions")) {
				Diseases::LoadDefinitions();
			}
			ImGuiMCP::SameLine();
			ImGuiMCP::TextDisabled("%s", Diseases::Describe().c_str());
			const auto defs = Diseases::Definitions();
			for (const auto& def : *defs) {
				bool shown = !Diseases::IsHidden(def.id);
				ImGuiMCP::PushID(def.id.c_str());
				if (ImGuiMCP::Checkbox(def.displayName.c_str(), std::addressof(shown))) {
					Diseases::SetHidden(def.id, !shown);
					Settings::Save();
				}
				ImGuiMCP::SameLine();
				ImGuiMCP::TextDisabled("%zu of %zu forms, %s", def.resolvedForms.size(), def.forms.size(), def.source.c_str());
				ImGuiMCP::PopID();
			}
		}

		void __stdcall TextPage()
		{
			auto& s = Settings::Get();
			ImGuiMCP::TextWrapped("Font, weight and outline of every text in the panel and in the SL Survival message panel.");
			ImGuiMCP::SeparatorText("Font");
			const auto& fonts = Typo::Available();
			const char* current = s.sFont.empty() ? "Game font (SKSE Menu Framework)" : s.sFont.c_str();
			ImGuiMCP::SetNextItemWidth(280.0f);
			if (ImGuiMCP::BeginCombo("Font", current)) {
				if (ImGuiMCP::Selectable("Game font (SKSE Menu Framework)", s.sFont.empty())) {
					s.sFont.clear();
					Settings::Save();
				}
				for (const auto& name : fonts) {
					if (ImGuiMCP::Selectable(name.c_str(), name == s.sFont)) {
						s.sFont = name;
						Settings::Save();
					}
				}
				ImGuiMCP::EndCombo();
			}
			Help("Fonts are the .ttf/.otf files in Data\\SKSE\\Plugins\\Fonts. SKSE Menu Framework reads that folder when the game starts, so a font added later shows up after a restart.");
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button("Refresh")) {
				Typo::Available(true);
			}
			if (!Typo::Loaded()) {
				ImGuiMCP::TextColored({ 1.0f, 0.55f, 0.35f, 1.0f }, "Not loaded by SKSE Menu Framework - restart the game. The game font is used meanwhile.");
			}
			Slider("Weight", s.fTextWeight, 0.0f, 2.0f, "%.2f px", "Makes any font bolder by drawing it a little wider. 0: the font as it is.");

			ImGuiMCP::SeparatorText("Outline");
			Slider("Thickness", s.fOutline, 0.0f, 4.0f, "%.2f px", "An outline around every letter so the text reads on snow and bright skies. 0: off (a soft drop shadow instead).");
			auto colour = Typo::ParseColour(s.sOutlineColor);
			if (ImGuiMCP::ColorEdit4("Colour", colour.data(), ImGuiMCP::ImGuiColorEditFlags_AlphaBar)) {
				s.sOutlineColor = Typo::FormatColour(colour);
				Settings::Save();
			}
			Help("The last value is the outline's opacity.");
			ImGuiMCP::TextWrapped("Sizes are pixels of the stage names at scale 1 and 1080p; bigger text gets a proportionally thicker outline.");
		}

		void __stdcall Messages()
		{
			auto& s = Settings::Get();
			ImGuiMCP::TextWrapped("SL Survival's messages (toll, licences, guards, curfew...) appear in their own panel instead of the game's notifications, stay longer and do not get lost among other mods' messages. The expand key shows the last 12.");
			ImGuiMCP::TextDisabled("%s", Feed::Describe().c_str());
			ImGuiMCP::Spacing();
			Toggle("Show SL Survival messages here", s.bMessages, "Takes effect after restarting the game. Off: SL Survival uses the game's notifications again.");
			Slider("Seconds on screen", s.fMsgSeconds, 3.0f, 60.0f, "%.0f s");
			if (ImGuiMCP::SliderInt("Lines", std::addressof(s.iMsgLines), 1, 12)) {
				Settings::Save();
			}
			Slider("Scale##msg", s.fMsgScale, 0.5f, 3.0f, "%.2fx");
			Slider("Width", s.fMsgWidth, 200.0f, 900.0f, "%.0f px");
			Slider("Background##msg", s.fMsgBackground, 0.0f, 0.85f, "%.2f");
			Slider("Position X##msg", s.fMsgPosX, 0.0f, 1.0f, "%.3f");
			Slider("Position Y##msg", s.fMsgPosY, 0.0f, 1.0f, "%.3f");
			ImGuiMCP::TextWrapped("With the preview on, the message panel shows sample lines and can be dragged with the mouse.");
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			return;
		}
		SKSEMenuFramework::SetSection("Vitals HUD");
		SKSEMenuFramework::AddSectionItem("Panel", Panel);
		SKSEMenuFramework::AddSectionItem("Text", TextPage);
		SKSEMenuFramework::AddSectionItem("Meters", Meters);
		SKSEMenuFramework::AddSectionItem("Diseases", DiseasesPage);
		SKSEMenuFramework::AddSectionItem("SL Survival messages", Messages);
	}
}
