#include "Hud.h"

#include "Diseases.h"
#include "Meters.h"
#include "Settings.h"
#include "Sls.h"
#include "Typo.h"

#pragma warning(push)
#pragma warning(disable : 4099 5054)
#include <SKSEMenuFramework.h>
#pragma warning(pop)

namespace Vitals::Hud
{
	namespace
	{
		using ImGuiMCP::ImU32;
		using ImGuiMCP::ImVec2;
		namespace DL = ImGuiMCP::ImDrawListManager;

		std::atomic_bool g_menuOpen{ false };
		std::atomic_bool g_preview{ true };
		std::atomic_bool g_expanded{ false };  // the detailed panel, toggled by the expand key over the compact one

		struct Rgba
		{
			float r, g, b;
		};

		Rgba Unpack(std::uint32_t a_rgb)
		{
			return { static_cast<float>(a_rgb & 0xFF), static_cast<float>((a_rgb >> 8) & 0xFF), static_cast<float>((a_rgb >> 16) & 0xFF) };
		}

		Rgba Mix(const Rgba& a_lhs, const Rgba& a_rhs, float a_t)
		{
			a_t = std::clamp(a_t, 0.0f, 1.0f);
			return { a_lhs.r + (a_rhs.r - a_lhs.r) * a_t, a_lhs.g + (a_rhs.g - a_lhs.g) * a_t, a_lhs.b + (a_rhs.b - a_lhs.b) * a_t };
		}

		ImU32 Col(const Rgba& a_c, float a_alpha)
		{
			return IM_COL32(static_cast<int>(a_c.r), static_cast<int>(a_c.g), static_cast<int>(a_c.b), static_cast<int>(std::clamp(a_alpha, 0.0f, 1.0f) * 255.0f));
		}

		ImU32 Col(int a_r, int a_g, int a_b, float a_alpha)
		{
			return IM_COL32(a_r, a_g, a_b, static_cast<int>(std::clamp(a_alpha, 0.0f, 1.0f) * 255.0f));
		}

		constexpr Rgba kCritical{ 235, 72, 58 };
		constexpr Rgba kGood{ 112, 222, 128 };
		constexpr Rgba kBad{ 255, 112, 76 };
		constexpr Rgba kWhite{ 245, 247, 250 };

		// per-meter animation state, render thread only
		struct Row
		{
			float shown{ 0.0f };   // fade/slide 0..1
			float fill{ -1.0f };   // eased bar
			float flash{ 0.0f };   // burst highlight
			bool  burst{ false };  // last frame's burst, to flash only when one starts
			std::chrono::steady_clock::time_point lastInteresting{};
		};
		std::array<Row, kMeterCount> g_rows;

		// what the preview shows for meters whose mod is missing or idle
		Snapshot Demo(MeterId a_id)
		{
			Snapshot s;
			s.available = true;
			s.nominal = false;
			switch (a_id) {
			case MeterId::kWetness:
				s.icon = "wet_sweaty";
				s.fill = 0.46f, s.stage = "Sweaty", s.value = "46%", s.trend = 1, s.marks = { 0.1f, 0.2f, 0.4f }, s.markCount = 3;
				break;
			case MeterId::kArousal:
				s.icon = "aro_lustful";
				s.fill = 0.72f, s.stage = "Lustful", s.value = "72", s.trend = 3, s.marks = { 0.2f, 0.4f, 0.6f, 0.8f }, s.markCount = 4;
				break;
			case MeterId::kDirt:
				s.icon = "dirt_slight";
				s.fill = 0.64f, s.stage = "Slightly dirty", s.value = "64%", s.trend = 1, s.severity = 0.33f, s.marks = { 0.2f, 0.6f, 0.98f }, s.markCount = 3;
				break;
			case MeterId::kWarmth:
				s.icon = "warm_verycold";
				s.fill = 0.58f, s.stage = "Very cold", s.value = "58%", s.trend = -2, s.severity = 0.5f, s.marks = { 0.2f, 0.5f, 0.7f, 0.88f, 0.95f }, s.markCount = 5;
				break;
			case MeterId::kHunger:
				s.icon = "food_peckish";
				s.fill = 0.36f, s.stage = "Peckish", s.value = "36%", s.trend = 1, s.severity = 0.25f, s.marks = { 0.08f, 0.16f, 0.34f, 0.52f, 0.77f }, s.markCount = 5;
				break;
			case MeterId::kFatigue:
				s.icon = "sleep_tired";
				s.fill = 0.6f, s.stage = "Tired", s.value = "60%", s.trend = -3, s.burst = true, s.severity = 0.5f, s.marks = { 0.08f, 0.17f, 0.35f, 0.58f, 0.83f }, s.markCount = 5;
				break;
			case MeterId::kCycle:
				s.icon = "bf_ovulation";
				s.fill = 0.4f, s.stage = "Ovulation", s.value = "40%", s.badge = "Fertile", s.severity = 0.5f;
				break;
			case MeterId::kCraving:
				s.icon = "crave_hungry";
				s.fill = 0.5f, s.stage = "Hungry", s.value = "50%", s.badge = "Addict", s.trend = 1, s.severity = 0.33f, s.marks = { 0.15f, 0.4f, 0.62f, 0.8f }, s.markCount = 4;
				break;
			default:
				break;
			}
			return s;
		}

		struct Fonts
		{
			ImGuiMCP::ImFont* text{ nullptr };
			ImGuiMCP::ImFont* icons{ nullptr };
		};

		Fonts GetFonts()
		{
			Fonts fonts;
			auto* base = ImGuiMCP::GetFont();
			FontAwesome::PushSolid();
			fonts.icons = ImGuiMCP::GetFont();
			FontAwesome::Pop();
			if (fonts.icons == base) {
				fonts.icons = nullptr;  // no FontAwesome: fall back to letters
			}
			fonts.text = Typo::Font();
			return fonts;
		}

		ImVec2 TextSize(ImGuiMCP::ImFont* a_font, float a_size, const char* a_text)
		{
			return ImGuiMCP::ImFontManger::CalcTextSizeA(a_font, a_size, FLT_MAX, 0.0f, a_text, nullptr, nullptr);
		}

		void Text(ImGuiMCP::ImDrawList* a_list, ImGuiMCP::ImFont* a_font, float a_size, ImVec2 a_pos, ImU32 a_col, float a_shadow, const char* a_text)
		{
			Typo::Draw(a_list, a_font, a_size, a_pos, a_col, a_text, 0.0f, a_shadow);
		}

		// stacked chevrons that stream in the direction of change; more chevrons = faster
		void DrawTrend(ImGuiMCP::ImDrawList* a_list, ImVec2 a_centre, float a_k, int a_trend, bool a_burst, const Rgba& a_colour, float a_alpha, float a_time)
		{
			const int count = std::min(3, std::abs(a_trend));
			if (count == 0) {
				// steady: a small dash so the column does not look empty
				DL::AddLine(a_list, { a_centre.x - 3.5f * a_k, a_centre.y }, { a_centre.x + 3.5f * a_k, a_centre.y }, Col(kWhite, 0.25f * a_alpha), std::max(1.0f, 1.5f * a_k));
				return;
			}
			const float dir = a_trend > 0 ? -1.0f : 1.0f;  // screen y grows downwards
			const float half = 5.0f * a_k;
			const float tall = 3.2f * a_k;
			const float gap = 4.6f * a_k;
			// one slow breath every ~5 s (a little quicker for more arrows); a burst breathes about twice as fast
			const float speed = a_burst ? 2.6f : 1.0f + 0.2f * static_cast<float>(count);
			const float thick = std::max(1.5f, 2.0f * a_k);
			const float span = gap * static_cast<float>(count - 1);
			for (int i = 0; i < count; ++i) {
				// chevron 0 is the trailing one; the wave runs from tail to head
				const float offset = -span * 0.5f + gap * static_cast<float>(i);
				const float y = a_centre.y - dir * offset;
				const float wave = 0.5f + 0.5f * std::sin(a_time * speed - static_cast<float>(i) * 0.9f);
				const float alpha = a_alpha * (0.62f + 0.38f * wave * wave * (3.0f - 2.0f * wave));
				const ImVec2 tip{ a_centre.x, y + dir * tall };
				const ImVec2 left{ a_centre.x - half, y - dir * tall };
				const ImVec2 right{ a_centre.x + half, y - dir * tall };
				const std::array<ImVec2, 3> shadow{ ImVec2{ left.x + 1, left.y + 1 }, ImVec2{ tip.x + 1, tip.y + 1 }, ImVec2{ right.x + 1, right.y + 1 } };
				DL::AddPolyline(a_list, shadow.data(), 3, Col(0, 0, 0, 0.45f * a_alpha), 0, thick);
				const std::array<ImVec2, 3> line{ left, tip, right };
				DL::AddPolyline(a_list, line.data(), 3, Col(a_colour, alpha), 0, thick);
			}
		}

		// stage pictures, loaded on first use through the framework (WIC reads the PNGs from the virtual Data folder)
		ImGuiMCP::ImTextureID Texture(const char* a_name)
		{
			static std::unordered_map<std::string, ImGuiMCP::ImTextureID> cache;
			if (!a_name) {
				return nullptr;
			}
			auto it = cache.find(a_name);
			if (it == cache.end()) {
				const auto path = std::format("Data/Interface/VitalsHUD/{}.png", a_name);
				auto       texture = SKSEMenuFramework::LoadTexture(path);
				if (!texture) {
					logger::warn("Could not load {}", path);
				}
				it = cache.emplace(a_name, texture).first;
			}
			return it->second;
		}

		// [Panel] fIconPlate: the dark disc under disease icons and letter fallbacks; 0 = no plate at all
		float PlateAlpha(const Settings& a_s)
		{
			const float plate = std::clamp(a_s.fIconPlate, 0.0f, 1.0f);
			return plate < 0.005f ? 0.0f : plate;
		}

		// [Panel] fIconSize: pictures larger than the text so the stage reads at a glance
		float IconSize(const Settings& a_s)
		{
			return std::clamp(a_s.fIconSize, 1.0f, 2.5f);
		}

		void DrawIcon(ImGuiMCP::ImDrawList* a_list, const Fonts& a_fonts, ImVec2 a_centre, float a_k, const MeterInfo& a_info, const char* a_icon, const Rgba& a_accent, float a_alpha,
			float a_pulse)
		{
			const float half = 14.0f * a_k * IconSize(Settings::Get());
			if (a_pulse > 0.0f) {
				// a slow, soft ring breathing out of a critical meter
				DL::AddCircle(a_list, a_centre, half + 1.0f * a_k + 5.0f * a_k * a_pulse, Col(kCritical, std::sin(a_pulse * 3.14159f) * 0.55f * a_alpha), 40, std::max(1.0f, 2.0f * a_k));
			}
			if (auto texture = Texture(a_icon)) {
				DL::AddImage(a_list, texture, { a_centre.x - half, a_centre.y - half }, { a_centre.x + half, a_centre.y + half }, { 0.0f, 0.0f }, { 1.0f, 1.0f }, Col(255, 255, 255, a_alpha));
				return;
			}
			const float radius = 11.0f * a_k;
			if (const float plate = PlateAlpha(Settings::Get()); plate > 0.0f) {
				DL::AddCircleFilled(a_list, a_centre, radius, Col(12, 15, 22, plate * a_alpha), 32);
			}
			DL::AddCircle(a_list, a_centre, radius, Col(a_accent, 0.9f * a_alpha), 32, std::max(1.0f, 1.5f * a_k));
			auto*       font = a_fonts.icons ? a_fonts.icons : a_fonts.text;
			const char  letter[2]{ a_info.title[0], 0 };
			const char* text = a_fonts.icons ? a_info.glyph : letter;
			const float size = 12.5f * a_k;
			const auto  extent = TextSize(font, size, text);
			Typo::Draw(a_list, font, size, { a_centre.x - extent.x * 0.5f, a_centre.y - extent.y * 0.5f }, Col(a_accent, a_alpha), text);
		}

		struct Layout
		{
			float k;
			float pad;
			float iconCell;
			float barW;
			float barH;
			float trendCell;
			float rowH;
			float tileW;
			bool  compact;
		};

		Layout MakeLayout(const Settings& a_s, float a_displayH, bool a_compact)
		{
			Layout l{};
			l.k = std::clamp(a_s.fScale, 0.4f, 3.0f) * (a_displayH / 1080.0f);
			l.pad = 8.0f * l.k;
			const float icon = 28.0f * IconSize(a_s);
			l.iconCell = (icon + 8.0f) * l.k;
			l.barW = std::clamp(a_s.fBarWidth, 60.0f, 400.0f) * l.k;
			l.barH = 7.0f * l.k;
			l.trendCell = 20.0f * l.k;
			l.compact = a_compact;
			if (a_compact) {
				l.rowH = std::max(34.0f, icon + 6.0f) * l.k;
				l.tileW = l.iconCell + l.trendCell;
			} else {
				l.rowH = std::max(a_s.bShowLabels ? 34.0f : 30.0f, icon + 6.0f) * l.k;
				l.tileW = l.iconCell + l.barW + l.trendCell;
			}
			return l;
		}

		// one meter: icon | label + value over the bar | trend column
		void DrawRow(ImGuiMCP::ImDrawList* a_list, const Fonts& a_fonts, const Layout& a_l, const Settings& a_s, ImVec2 a_origin, MeterId a_id, const Snapshot& a_snap, Row& a_row,
			float a_alpha, float a_time)
		{
			const auto& info = Info(a_id);
			const Rgba  accent = Unpack(info.accent);
			const float k = a_l.k;
			const float midY = a_origin.y + a_l.rowH * 0.5f;

			const float pulse = a_snap.severity >= 0.74f ? std::fmod(a_time / 2.8f, 1.0f) : 0.0f;
			DrawIcon(a_list, a_fonts, { a_origin.x + a_l.iconCell * 0.5f - 2.0f * k, midY }, k, info, a_snap.icon, accent, a_alpha, pulse);

			const float barX = a_origin.x + a_l.iconCell;
			const float barY = a_s.bShowLabels ? a_origin.y + a_l.rowH - a_l.barH - 4.0f * k : midY - a_l.barH * 0.5f;
			const ImVec2 barMin{ barX, barY };
			const ImVec2 barMax{ barX + a_l.barW, barY + a_l.barH };
			const float  round = a_l.barH * 0.5f;

			if (a_s.bShowLabels) {
				const float textSize = 14.5f * k;
				const float smallSize = 12.5f * k;
				const Rgba  textColour = Mix(kWhite, Mix(kWhite, kCritical, 0.55f), a_snap.severity >= 0.74f ? 1.0f : 0.0f);
				const float textY = barY - textSize - 2.0f * k;
				Text(a_list, a_fonts.text, textSize, { barX, textY }, Col(textColour, 0.95f * a_alpha), 0.7f * a_alpha, a_snap.stage.c_str());
				float cursor = barX + TextSize(a_fonts.text, textSize, a_snap.stage.c_str()).x + 6.0f * k;
				if (!a_snap.badge.empty()) {
					const auto   extent = TextSize(a_fonts.text, smallSize, a_snap.badge.c_str());
					const ImVec2 pillMin{ cursor, textY + (textSize - smallSize) * 0.5f - 1.0f * k };
					const ImVec2 pillMax{ cursor + extent.x + 8.0f * k, pillMin.y + smallSize + 2.0f * k };
					DL::AddRectFilled(a_list, pillMin, pillMax, Col(accent, 0.22f * a_alpha), (pillMax.y - pillMin.y) * 0.5f, ImGuiMCP::ImDrawFlags_RoundCornersAll);
					DL::AddRect(a_list, pillMin, pillMax, Col(accent, 0.75f * a_alpha), (pillMax.y - pillMin.y) * 0.5f, ImGuiMCP::ImDrawFlags_RoundCornersAll, std::max(1.0f, k));
					Typo::Draw(a_list, a_fonts.text, smallSize, { cursor + 4.0f * k, pillMin.y + 1.0f * k }, Col(kWhite, 0.95f * a_alpha), a_snap.badge.c_str());
				}
				if (a_s.bShowValues && !a_snap.value.empty()) {
					const auto extent = TextSize(a_fonts.text, smallSize, a_snap.value.c_str());
					Text(a_list, a_fonts.text, smallSize, { barMax.x - extent.x, textY + (textSize - smallSize) }, Col(kWhite, 0.6f * a_alpha), 0.6f * a_alpha, a_snap.value.c_str());
				}
			}

			// track, fill, stage marks, a soft top highlight
			DL::AddRectFilled(a_list, { barMin.x - 1.0f, barMin.y - 1.0f }, { barMax.x + 1.0f, barMax.y + 1.0f }, Col(0, 0, 0, 0.45f * a_alpha), round + 1.0f, ImGuiMCP::ImDrawFlags_RoundCornersAll);
			DL::AddRectFilled(a_list, barMin, barMax, Col(30, 34, 44, 0.85f * a_alpha), round, ImGuiMCP::ImDrawFlags_RoundCornersAll);
			const float fill = std::clamp(a_row.fill, 0.0f, 1.0f);
			const Rgba  fillColour = Mix(accent, kCritical, info.better == Better::kNeither ? 0.0f : a_snap.severity * 0.8f);
			if (fill > 0.002f) {
				const float  w = std::max(a_l.barH, a_l.barW * fill);
				const ImVec2 fillMax{ barMin.x + w, barMax.y };
				DL::AddRectFilled(a_list, barMin, fillMax, Col(fillColour, 0.95f * a_alpha), round, ImGuiMCP::ImDrawFlags_RoundCornersAll);
				DL::AddRectFilled(a_list, { barMin.x + round * 0.5f, barMin.y + 1.0f * k }, { fillMax.x - round * 0.5f, barMin.y + a_l.barH * 0.42f }, Col(kWhite, 0.22f * a_alpha), round * 0.5f,
					ImGuiMCP::ImDrawFlags_RoundCornersAll);
				if (a_row.flash > 0.01f) {
					DL::AddRectFilled(a_list, barMin, fillMax, Col(kWhite, 0.3f * a_row.flash * a_row.flash * a_alpha), round, ImGuiMCP::ImDrawFlags_RoundCornersAll);
				}
			}
			for (std::uint8_t i = 0; i < a_snap.markCount; ++i) {
				const float x = barMin.x + a_l.barW * a_snap.marks[i];
				DL::AddLine(a_list, { x, barMin.y + 1.0f }, { x, barMax.y - 1.0f }, Col(8, 10, 14, 0.75f * a_alpha), std::max(1.0f, 1.2f * k));
			}

			if (a_id != MeterId::kCycle) {
				Rgba arrow = Mix(accent, kWhite, 0.25f);
				if (info.better != Better::kNeither && a_snap.trend != 0) {
					const bool improving = (a_snap.trend > 0) == (info.better == Better::kUp);
					arrow = improving ? kGood : kBad;
				}
				DrawTrend(a_list, { barMax.x + a_l.trendCell * 0.5f + 2.0f * k, a_s.bShowLabels ? midY + 2.0f * k : midY }, k, a_snap.trend, a_snap.burst, arrow, a_alpha, a_time);
			}
		}

		Rgba ArrowColour(const MeterInfo& a_info, const Snapshot& a_snap)
		{
			const Rgba accent = Unpack(a_info.accent);
			if (a_info.better != Better::kNeither && a_snap.trend != 0) {
				const bool improving = (a_snap.trend > 0) == (a_info.better == Better::kUp);
				return improving ? kGood : kBad;
			}
			return Mix(accent, kWhite, 0.25f);
		}

		// compact: the stage picture and the trend column, nothing else
		void DrawTile(ImGuiMCP::ImDrawList* a_list, const Fonts& a_fonts, const Layout& a_l, ImVec2 a_origin, MeterId a_id, const Snapshot& a_snap, float a_alpha, float a_time)
		{
			const auto& info = Info(a_id);
			const float k = a_l.k;
			const float midY = a_origin.y + a_l.rowH * 0.5f;
			const float pulse = a_snap.severity >= 0.74f ? std::fmod(a_time / 2.8f, 1.0f) : 0.0f;
			DrawIcon(a_list, a_fonts, { a_origin.x + a_l.iconCell * 0.5f - 2.0f * k, midY }, k, info, a_snap.icon, Unpack(info.accent), a_alpha, pulse);
			if (a_id != MeterId::kCycle) {
				DrawTrend(a_list, { a_origin.x + a_l.iconCell + a_l.trendCell * 0.5f - 2.0f * k, midY }, k, a_snap.trend, a_snap.burst, ArrowColour(info, a_snap), a_alpha, a_time);
			}
		}

		// ---- SL Survival status block ---------------------------------------------------------------------------

		Rgba ToneColour(Sls::Tone a_tone)
		{
			switch (a_tone) {
			case Sls::Tone::kGood:
				return kGood;
			case Sls::Tone::kWarn:
				return { 246, 196, 84 };
			case Sls::Tone::kBad:
				return kBad;
			default:
				return { 190, 196, 208 };
			}
		}

		std::vector<Sls::Item> DemoStatus()
		{
			return {
				{ "\xef\x8b\x82", "Weapon licence", "valid, 3 d 4 h left", "Wpn 3d", Sls::Tone::kGood, "lic_weapon" },
				{ "\xef\x8b\x82", "Armor licence", "none - you carry contraband", "Arm !", Sls::Tone::kBad, "lic_armor" },
				{ "\xef\x94\x9e", "Toll, Whiterun", "not paid (100 gold)", "Toll 100", Sls::Tone::kBad, "toll_unpaid" },
				{ "\xef\x86\x86", "Curfew", "in effect until 06:00", "Curfew", Sls::Tone::kWarn, "curfew_on" },
				{ "\xef\x89\x83", "Orgasm fatigue", "tired (4.0 of 3.0 today)", "Fatigue 1", Sls::Tone::kWarn, "ofat_1" },
			};
		}

		struct StatusLayout
		{
			float rowH{ 0.0f };
			float glyphCell{ 0.0f };
			float textSize{ 0.0f };
			float labelW{ 0.0f };
			float valueW{ 0.0f };
			float width{ 0.0f };
			float height{ 0.0f };
		};

		// compact: one chip per item (glyph + short tag); detailed: glyph | label | value
		StatusLayout MeasureStatus(const std::vector<Sls::Item>& a_items, const Fonts& a_fonts, const Layout& a_l, bool a_horizontal)
		{
			StatusLayout sl;
			const float k = a_l.k;
			sl.textSize = (a_l.compact ? 12.5f : 13.5f) * k;
			const float icon = 18.0f * IconSize(Settings::Get());
			sl.rowH = std::max(a_l.compact ? 20.0f : 21.0f, icon + 4.0f) * k;
			sl.glyphCell = (icon + 4.0f) * k;
			for (const auto& item : a_items) {
				if (a_l.compact) {
					sl.labelW = std::max(sl.labelW, TextSize(a_fonts.text, sl.textSize, item.tag.c_str()).x);
				} else {
					sl.labelW = std::max(sl.labelW, TextSize(a_fonts.text, sl.textSize, item.label.c_str()).x);
					sl.valueW = std::max(sl.valueW, TextSize(a_fonts.text, sl.textSize, item.value.c_str()).x);
				}
			}
			const float count = static_cast<float>(a_items.size());
			if (a_l.compact) {
				const float chipW = sl.glyphCell + sl.labelW + 8.0f * k;
				sl.width = a_horizontal ? count * chipW + (count - 1.0f) * 6.0f * k : chipW;
				sl.height = a_horizontal ? sl.rowH : count * sl.rowH;
			} else {
				sl.width = sl.glyphCell + sl.labelW + 12.0f * k + sl.valueW;
				sl.height = count * sl.rowH;
			}
			return sl;
		}

		void DrawStatus(ImGuiMCP::ImDrawList* a_list, const Fonts& a_fonts, const Layout& a_l, const StatusLayout& a_sl, ImVec2 a_origin, const std::vector<Sls::Item>& a_items, bool a_horizontal,
			float a_alpha)
		{
			const float k = a_l.k;
			auto*       glyphFont = a_fonts.icons ? a_fonts.icons : a_fonts.text;
			float       x = a_origin.x;
			float       y = a_origin.y;
			for (const auto& item : a_items) {
				const Rgba  tone = ToneColour(item.tone);
				const float midY = y + a_sl.rowH * 0.5f;
				if (auto texture = item.icon ? Texture(item.icon) : nullptr) {
					const float half = 9.0f * k * IconSize(Settings::Get());
					const float cx = x + a_sl.glyphCell * 0.5f;
					DL::AddImage(a_list, texture, { cx - half, midY - half }, { cx + half, midY + half }, { 0.0f, 0.0f }, { 1.0f, 1.0f },
						item.dim ? Col(150, 150, 150, 0.45f * a_alpha) : Col(255, 255, 255, a_alpha));
				} else {
					const char* glyph = a_fonts.icons && item.glyph ? item.glyph : "*";
					const float glyphSize = 12.5f * k;
					const auto  ge = TextSize(glyphFont, glyphSize, glyph);
					Text(a_list, glyphFont, glyphSize, { x + (a_sl.glyphCell - ge.x) * 0.5f, midY - ge.y * 0.5f }, Col(tone, 0.95f * a_alpha), 0.6f * a_alpha, glyph);
				}
				const float textY = midY - a_sl.textSize * 0.5f - 1.0f * k;
				if (a_l.compact) {
					Text(a_list, a_fonts.text, a_sl.textSize, { x + a_sl.glyphCell, textY }, Col(Mix(kWhite, tone, 0.45f), 0.92f * a_alpha), 0.6f * a_alpha, item.tag.c_str());
					if (a_horizontal) {
						x += a_sl.glyphCell + TextSize(a_fonts.text, a_sl.textSize, item.tag.c_str()).x + 14.0f * k;
					} else {
						y += a_sl.rowH;
					}
				} else {
					Text(a_list, a_fonts.text, a_sl.textSize, { x + a_sl.glyphCell, textY }, Col(kWhite, 0.9f * a_alpha), 0.6f * a_alpha, item.label.c_str());
					Text(a_list, a_fonts.text, a_sl.textSize, { x + a_sl.glyphCell + a_sl.labelW + 12.0f * k, textY }, Col(tone, 0.95f * a_alpha), 0.6f * a_alpha, item.value.c_str());
					y += a_sl.rowH;
				}
			}
		}

		// ---- Disease block (from Disease Widget - SKSE by kuroishitam, with permission) -------------------------

		// white glyphs tinted at runtime: SVG at the size it is drawn, PNG fallback, then Disease Widget's own folder
		// (where other mods ship their icons), then the generic icon
		ImGuiMCP::ImTextureID DiseaseTexture(const std::string& a_icon, float a_px)
		{
			static std::unordered_map<std::string, ImGuiMCP::ImTextureID> cache;
			const int bucket = a_px <= 64.0f ? 64 : a_px <= 128.0f ? 128 : 256;
			const auto key = std::format("{}@{}", a_icon, bucket);
			if (const auto it = cache.find(key); it != cache.end()) {
				return it->second;
			}
			auto load = [&](const std::string& a_name) -> ImGuiMCP::ImTextureID {
				for (const auto* folder : { "Data/Interface/VitalsHUD/disease/", "Data/Interface/DiseaseWidget/Icons/" }) {
					const auto base = std::string(folder) + a_name;
					if (auto texture = SKSEMenuFramework::LoadTexture(base + ".svg", ImVec2{ static_cast<float>(bucket), static_cast<float>(bucket) })) {
						return texture;
					}
					if (auto texture = SKSEMenuFramework::LoadTexture(base + ".png")) {
						return texture;
					}
				}
				return nullptr;
			};
			auto texture = load(a_icon);
			if (!texture && a_icon != "generic") {
				logger::warn("Disease icon '{}' not found, using the generic one", a_icon);
				texture = load("generic");
			}
			cache.emplace(key, texture);  // failures too, so the disk is not hit every frame
			return texture;
		}

		constexpr std::array<float, 4> kDiseaseIcon{ 0.93f, 0.89f, 0.80f, 1.0f };
		constexpr Rgba                 kDiseaseRing{ 199, 184, 140 };
		constexpr Rgba                 kDiseaseStage{ 246, 196, 84 };

		Rgba DiseaseTint(const Diseases::Active& a_d, const Settings& a_s)
		{
			const auto& c = a_s.bDiseaseColors && a_d.hasTint ? a_d.tint : kDiseaseIcon;
			return { c[0] * 255.0f, c[1] * 255.0f, c[2] * 255.0f };
		}

		// the preview shows the first three diseases you have not switched off
		std::vector<Diseases::Active> DemoDiseases()
		{
			std::vector<Diseases::Active> out;
			const auto                    defs = Diseases::Definitions();
			for (const auto& def : *defs) {
				if (out.size() >= 3) {
					break;
				}
				if (Diseases::IsHidden(def.id)) {
					continue;
				}
				Diseases::Active d;
				d.id = def.id;
				d.name = def.displayName;
				d.icon = def.icon;
				d.tint = def.tint;
				d.hasTint = def.hasTint;
				d.firstSeen = Diseases::NowSeconds() - 100.0;
				if (out.empty()) {
					d.stage = "Mild " + def.displayName;
				}
				out.push_back(std::move(d));
			}
			return out;
		}

		struct DiseaseLayout
		{
			float rowH{ 0.0f };
			float iconCell{ 0.0f };
			float textSize{ 0.0f };
			float nameW{ 0.0f };
			float stageW{ 0.0f };
			float width{ 0.0f };
			float height{ 0.0f };
		};

		// compact: one chip per disease (icon + name); detailed: icon | name | stage
		DiseaseLayout MeasureDiseases(const std::vector<Diseases::Active>& a_items, const Fonts& a_fonts, const Layout& a_l, bool a_horizontal)
		{
			DiseaseLayout dl;
			const float   k = a_l.k;
			dl.textSize = (a_l.compact ? 12.5f : 13.5f) * k;
			const float icon = 20.0f * IconSize(Settings::Get());
			dl.rowH = (icon + 4.0f) * k;
			dl.iconCell = (icon + 6.0f) * k;
			for (const auto& d : a_items) {
				dl.nameW = std::max(dl.nameW, TextSize(a_fonts.text, dl.textSize, d.name.c_str()).x);
				if (!a_l.compact) {
					dl.stageW = std::max(dl.stageW, TextSize(a_fonts.text, dl.textSize, d.stage.c_str()).x);
				}
			}
			const float count = static_cast<float>(a_items.size());
			if (a_l.compact) {
				if (a_horizontal) {
					for (const auto& d : a_items) {
						dl.width += dl.iconCell + TextSize(a_fonts.text, dl.textSize, d.name.c_str()).x + 14.0f * k;
					}
					dl.width -= 14.0f * k;
					dl.height = dl.rowH;
				} else {
					dl.width = dl.iconCell + dl.nameW + 8.0f * k;
					dl.height = count * dl.rowH;
				}
			} else {
				dl.width = dl.iconCell + dl.nameW + (dl.stageW > 0.0f ? 12.0f * k + dl.stageW : 0.0f);
				dl.height = count * dl.rowH;
			}
			return dl;
		}

		// round Skyrim-style plate with a double ring and the tinted glyph; new diseases pulse for 5 s
		void DrawDiseaseIcon(ImGuiMCP::ImDrawList* a_list, ImVec2 a_centre, float a_half, float a_k, const Diseases::Active& a_d, const Settings& a_s, float a_alpha, bool a_dim)
		{
			const double age = Diseases::NowSeconds() - a_d.firstSeen;
			float        ringAlpha = a_alpha * (a_dim ? 0.4f : 1.0f);
			if (a_s.bDiseasePulse && age < 5.0) {
				ringAlpha *= 0.7f + 0.3f * static_cast<float>(std::sin(age * 6.0));
			}
			const float thick = std::max(1.0f, 1.6f * a_k);
			if (const float plate = PlateAlpha(a_s); plate > 0.0f) {
				DL::AddCircleFilled(a_list, a_centre, a_half, Col(13, 12, 10, plate * a_alpha), 40);
			}
			DL::AddCircle(a_list, a_centre, a_half - thick * 0.5f, Col(kDiseaseRing, 0.85f * ringAlpha), 40, thick);
			DL::AddCircle(a_list, a_centre, a_half * 0.9f, Col(kDiseaseRing, 0.38f * ringAlpha), 40, std::max(1.0f, thick * 0.6f));
			const float glyph = a_half * 0.72f;
			if (auto texture = DiseaseTexture(a_d.icon, glyph * 2.0f)) {
				const Rgba tint = a_dim ? Rgba{ 150, 150, 150 } : DiseaseTint(a_d, a_s);
				DL::AddImage(a_list, texture, { a_centre.x - glyph, a_centre.y - glyph }, { a_centre.x + glyph, a_centre.y + glyph }, { 0.0f, 0.0f }, { 1.0f, 1.0f },
					Col(tint, (a_dim ? 0.5f : 1.0f) * a_alpha));
			}
		}

		void DrawDiseases(ImGuiMCP::ImDrawList* a_list, const Fonts& a_fonts, const Layout& a_l, const DiseaseLayout& a_dl, ImVec2 a_origin, const std::vector<Diseases::Active>& a_items,
			bool a_horizontal, float a_alpha)
		{
			const auto& s = Settings::Get();
			const float k = a_l.k;
			float       x = a_origin.x;
			float       y = a_origin.y;
			const float half = 10.0f * k * IconSize(s);
			for (const auto& d : a_items) {
				const bool  dim = d.id.empty();  // the "No diseases" line of Always
				const float midY = y + a_dl.rowH * 0.5f;
				DrawDiseaseIcon(a_list, { x + a_dl.iconCell * 0.5f, midY }, half, k, d, s, a_alpha, dim);
				const float textY = midY - a_dl.textSize * 0.5f - 1.0f * k;
				const Rgba  nameColour = dim ? Rgba{ 160, 164, 172 } : (a_l.compact ? Mix(kWhite, DiseaseTint(d, s), 0.4f) : kWhite);
				Text(a_list, a_fonts.text, a_dl.textSize, { x + a_dl.iconCell, textY }, Col(nameColour, 0.92f * a_alpha), 0.6f * a_alpha, d.name.c_str());
				if (a_l.compact && a_horizontal) {
					x += a_dl.iconCell + TextSize(a_fonts.text, a_dl.textSize, d.name.c_str()).x + 14.0f * k;
					continue;
				}
				if (!a_l.compact && !d.stage.empty()) {
					Text(a_list, a_fonts.text, a_dl.textSize, { x + a_dl.iconCell + a_dl.nameW + 12.0f * k, textY }, Col(kDiseaseStage, 0.95f * a_alpha), 0.6f * a_alpha, d.stage.c_str());
				}
				y += a_dl.rowH;
			}
		}

		bool Wanted(MeterId a_id, const Snapshot& a_snap, Row& a_row, const Settings& a_s, bool a_preview, std::chrono::steady_clock::time_point a_now)
		{
			const auto mode = Visibility(a_id);
			if (mode == 2) {
				return false;
			}
			if (a_preview) {
				return true;
			}
			if (!a_snap.available) {
				return false;
			}
			if (mode == 1) {
				return true;
			}
			if (!a_snap.nominal || a_snap.trend != 0) {
				a_row.lastInteresting = a_now;
				return true;
			}
			return a_now - a_row.lastInteresting < std::chrono::duration<float>(std::max(0.0f, a_s.fLinger));
		}

		void __stdcall Draw()
		{
			const auto& s = Settings::Get();
			auto*       io = ImGuiMCP::GetIO();
			auto*       ui = RE::UI::GetSingleton();
			if (!io || !ui) {
				return;
			}
			// own frame clock: the framework's DeltaTime is not guaranteed to advance while its menu is closed
			const auto  now = std::chrono::steady_clock::now();
			static auto lastFrame = now;
			const float dt = std::clamp(std::chrono::duration<float>(now - lastFrame).count(), 0.0f, 0.1f);
			lastFrame = now;
			const float time = static_cast<float>(ImGuiMCP::GetTime());

			Context    context;
			auto       snaps = Meters::Read(context);
			const bool preview = g_menuOpen && g_preview;

			bool hidden = !s.bEnabled || !context.ready;
			if (!preview) {
				hidden = hidden || context.menu || !ui->IsShowingMenus() || (s.bHideInCombat && context.combat) || (s.bHideInScenes && context.scene) ||
				         (s.bHideInDialogue && context.dialogue);
			} else {
				hidden = !s.bEnabled;
			}

			if (preview) {
				for (std::size_t i = 0; i < kMeterCount; ++i) {
					if (!snaps[i].available || snaps[i].nominal) {
						snaps[i] = Demo(static_cast<MeterId>(i));
					}
				}
			}

			const Layout l = MakeLayout(s, io->DisplaySize.y, s.bCompact && !g_expanded);
			std::array<bool, kMeterCount> wanted{};
			float                         extent = 0.0f;  // rows (vertical) or tiles (horizontal), weighted by fade
			float                         maxShown = 0.0f;
			for (std::size_t i = 0; i < kMeterCount; ++i) {
				auto& row = g_rows[i];
				wanted[i] = !hidden && Wanted(static_cast<MeterId>(i), snaps[i], row, s, preview, now);
				row.shown += ((wanted[i] ? 1.0f : 0.0f) - row.shown) * std::min(1.0f, dt * 7.0f);
				if (row.shown < 0.003f) {
					row.shown = 0.0f;
					row.fill = -1.0f;
					continue;
				}
				const float target = std::clamp(snaps[i].fill, 0.0f, 1.0f);
				row.fill = row.fill < 0.0f ? target : row.fill + (target - row.fill) * std::min(1.0f, dt * 5.0f);
				if (snaps[i].burst && !row.burst) {
					row.flash = 1.0f;
				}
				row.burst = snaps[i].burst;
				row.flash = std::max(0.0f, row.flash - dt * 0.6f);
				extent += row.shown;
				maxShown = std::max(maxShown, row.shown);
			}
			// diagnostics: a short state line every 5 s, the first dozen times
			static int  reports = 0;
			static auto nextReport = now;
			if (reports < 12 && now >= nextReport) {
				++reports;
				nextReport = now + std::chrono::seconds(5);
				std::string meters;
				for (std::size_t i = 0; i < kMeterCount; ++i) {
					meters += std::format(" {}={}{}{}:{:.2f}", Info(static_cast<MeterId>(i)).key, snaps[i].available ? 'a' : '-', snaps[i].nominal ? 'n' : '-', wanted[i] ? 'w' : '-', g_rows[i].shown);
				}
				logger::info("draw: ready={} enabled={} gameMenu={} combat={} scene={} dialogue={} showing={} preview={} smf={} |{}", context.ready, s.bEnabled, context.menu, context.combat,
					context.scene, context.dialogue, ui->IsShowingMenus(), preview, g_menuOpen.load(), meters);
			}
			// SL Survival status, faded as one block
			static float statusShown = 0.0f;
			auto         status = preview && Sls::Read().empty() ? DemoStatus() : Sls::Read();
			if (l.compact) {
				std::erase_if(status, [](const Sls::Item& a_item) { return a_item.detailOnly; });
			}
			const bool   statusWanted = !hidden && !status.empty();
			statusShown += ((statusWanted ? 1.0f : 0.0f) - statusShown) * std::min(1.0f, dt * 7.0f);
			if (statusShown < 0.003f) {
				statusShown = 0.0f;
			}
			static std::vector<Sls::Item> lastStatus;  // keep drawing the last items while the block fades out
			if (statusWanted) {
				lastStatus = status;
			} else {
				status = lastStatus;
			}
			// diseases, faded as one block under the SL Survival status
			static float diseaseShown = 0.0f;
			std::vector<Diseases::Active> diseases;
			if (s.iDiseases != 2) {
				diseases = Diseases::Read();
				if (diseases.empty() && preview) {
					diseases = DemoDiseases();
				}
				if (diseases.empty() && s.iDiseases == 1) {
					Diseases::Active healthy;
					healthy.name = "No diseases";
					healthy.icon = "generic";
					diseases.push_back(std::move(healthy));
				}
			}
			const bool diseasesWanted = !hidden && !diseases.empty();
			diseaseShown += ((diseasesWanted ? 1.0f : 0.0f) - diseaseShown) * std::min(1.0f, dt * 7.0f);
			if (diseaseShown < 0.003f) {
				diseaseShown = 0.0f;
			}
			static std::vector<Diseases::Active> lastDiseases;
			if (diseasesWanted) {
				lastDiseases = diseases;
			} else {
				diseases = lastDiseases;
			}
			if (maxShown <= 0.0f && statusShown <= 0.0f && diseaseShown <= 0.0f) {
				return;
			}
			const auto fonts = GetFonts();

			const float alpha = std::clamp(s.fOpacity, 0.1f, 1.0f);
			const float gap = 4.0f * l.k;
			const float tileGap = (l.compact ? 6.0f : 12.0f) * l.k;
			const float metersSpan = s.bHorizontal ? extent * (l.tileW + tileGap) - tileGap * std::min(1.0f, extent) : extent * (l.rowH + gap) - gap * std::min(1.0f, extent);
			float       width, height;
			if (s.bHorizontal) {
				width = l.pad * 2.0f + metersSpan;
				height = l.pad * 2.0f + l.rowH * maxShown;
			} else {
				width = l.pad * 2.0f + l.tileW;
				height = l.pad * 2.0f + metersSpan;
			}
			const StatusLayout sl = MeasureStatus(status, fonts, l, s.bHorizontal);
			const float        statusGap = maxShown > 0.0f ? 8.0f * l.k : 0.0f;
			if (statusShown > 0.0f && !status.empty()) {
				width = std::max(width, l.pad * 2.0f + sl.width);
				height += (statusGap + sl.height) * statusShown;
			}
			const bool          statusDrawn = statusShown > 0.0f && !status.empty();
			const DiseaseLayout dl = MeasureDiseases(diseases, fonts, l, s.bHorizontal);
			const float         diseaseGap = maxShown > 0.0f || statusDrawn ? 8.0f * l.k : 0.0f;
			const bool          diseasesDrawn = diseaseShown > 0.0f && !diseases.empty();
			if (diseasesDrawn) {
				width = std::max(width, l.pad * 2.0f + dl.width);
				height += (diseaseGap + dl.height) * diseaseShown;
			}

			float x = std::clamp(s.fPosX, 0.0f, 1.0f) * io->DisplaySize.x;
			float y = std::clamp(s.fPosY, 0.0f, 1.0f) * io->DisplaySize.y;
			if (s.bAlignRight) {
				x -= width;
			}

			// in preview the panel can be dragged with the mouse when no menu window is under the cursor
			static bool   dragging = false;
			static ImVec2 grab{};
			if (preview) {
				const bool inside = io->MousePos.x >= x && io->MousePos.x <= x + width && io->MousePos.y >= y && io->MousePos.y <= y + height;
				if (!dragging && io->MouseDown[0] && inside && !ImGuiMCP::IsWindowHovered(ImGuiMCP::ImGuiHoveredFlags_AnyWindow)) {
					dragging = true;
					grab = { io->MousePos.x - x, io->MousePos.y - y };
				}
				if (dragging) {
					if (io->MouseDown[0]) {
						auto& edit = Settings::Get();
						const float nx = io->MousePos.x - grab.x + (s.bAlignRight ? width : 0.0f);
						edit.fPosX = std::clamp(nx / io->DisplaySize.x, 0.0f, 1.0f);
						edit.fPosY = std::clamp((io->MousePos.y - grab.y) / io->DisplaySize.y, 0.0f, 1.0f);
					} else {
						dragging = false;
						Settings::Save();
					}
				}
			} else {
				dragging = false;
			}

			auto* list = ImGuiMCP::GetBackgroundDrawList();  // under every ImGui window, so the SMF menu covers it
			const float bgAlpha = std::clamp(s.fBackground, 0.0f, 1.0f) * alpha * std::max({ maxShown, statusShown, diseaseShown });
			// the preview shows the background exactly as set (0 = none); only its frame marks where to drag
			if (bgAlpha > 0.005f) {
				DL::AddRectFilled(list, { x, y }, { x + width, y + height }, Col(8, 10, 16, bgAlpha), 9.0f * l.k, ImGuiMCP::ImDrawFlags_RoundCornersAll);
			}
			if (preview) {
				DL::AddRect(list, { x, y }, { x + width, y + height }, Col(255, 255, 255, dragging ? 0.7f : 0.3f), 9.0f * l.k, ImGuiMCP::ImDrawFlags_RoundCornersAll, std::max(1.0f, l.k));
			}

			float cursor = s.bHorizontal ? x + l.pad : y + l.pad;
			for (std::size_t i = 0; i < kMeterCount; ++i) {
				auto& row = g_rows[i];
				if (row.shown <= 0.0f) {
					continue;
				}
				const float  rowAlpha = alpha * row.shown * row.shown;
				const ImVec2 origin = s.bHorizontal ? ImVec2{ cursor, y + l.pad } : ImVec2{ x + l.pad, cursor };
				if (l.compact) {
					DrawTile(list, fonts, l, origin, static_cast<MeterId>(i), snaps[i], rowAlpha, time);
				} else {
					DrawRow(list, fonts, l, s, origin, static_cast<MeterId>(i), snaps[i], row, rowAlpha, time);
				}
				cursor += s.bHorizontal ? (l.tileW + tileGap) * row.shown : (l.rowH + gap) * row.shown;
			}

			float top = s.bHorizontal ? y + l.pad + l.rowH * maxShown : cursor - (maxShown > 0.0f ? gap : 0.0f);
			if (statusDrawn) {
				if (maxShown > 0.0f) {
					const float lineY = top + statusGap * 0.5f;
					DL::AddLine(list, { x + l.pad, lineY }, { x + width - l.pad, lineY }, Col(kWhite, 0.12f * alpha * statusShown), std::max(1.0f, l.k));
				}
				DrawStatus(list, fonts, l, sl, { x + l.pad, top + statusGap }, status, s.bHorizontal, alpha * statusShown * statusShown);
				top += (statusGap + sl.height) * statusShown;
			}
			if (diseasesDrawn) {
				if (diseaseGap > 0.0f) {
					const float lineY = top + diseaseGap * 0.5f;
					DL::AddLine(list, { x + l.pad, lineY }, { x + width - l.pad, lineY }, Col(kWhite, 0.12f * alpha * diseaseShown), std::max(1.0f, l.k));
				}
				DrawDiseases(list, fonts, l, dl, { x + l.pad, top + diseaseGap }, diseases, s.bHorizontal, alpha * diseaseShown * diseaseShown);
			}
		}

		void __stdcall OnMenuEvent(SKSEMenuFramework::Model::EventType a_type)
		{
			if (a_type == SKSEMenuFramework::Model::EventType::kOpenMenu) {
				g_menuOpen = true;
			} else if (a_type == SKSEMenuFramework::Model::EventType::kCloseMenu) {
				g_menuOpen = false;
			}
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			logger::warn("SKSE Menu Framework is not installed: no HUD");
			return;
		}
		static auto* element = SKSEMenuFramework::AddHudElement(Draw);
		static auto* event = SKSEMenuFramework::AddEvent(OnMenuEvent, 0.0f);
		(void)element;
		(void)event;
		logger::info("Registered the HUD panel");
	}

	void ToggleExpanded()
	{
		g_expanded = !g_expanded;
	}

	bool Expanded()
	{
		return g_expanded;
	}

	bool MenuOpen()
	{
		return g_menuOpen;
	}

	bool PreviewEnabled()
	{
		return g_preview;
	}

	void SetPreview(bool a_enabled)
	{
		g_preview = a_enabled;
	}
}
