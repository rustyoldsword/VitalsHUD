#include "Feed.h"

#include "Hud.h"
#include "Meters.h"
#include "SLSFW_HudStatus.h"
#include "Settings.h"
#include "Typo.h"

#include <Windows.h>

#include <charconv>

#pragma warning(push)
#pragma warning(disable : 4099 5054)
#include <SKSEMenuFramework.h>
#pragma warning(pop)

namespace Vitals::Feed
{
	namespace
	{
		using ImGuiMCP::ImU32;
		using ImGuiMCP::ImVec2;
		namespace DL = ImGuiMCP::ImDrawListManager;
		using Clock = std::chrono::steady_clock;

		struct Line
		{
			std::uint32_t     id;
			std::string       text;
			Clock::time_point arrived;
			int               colour{ -1 };  // 0xRRGGBB from a <font color='#...'> tag, -1 = default
		};

		// SL Survival writes the same markup the game's notifications understand: <font color='#CC0000'>...</font>.
		// Drop every tag and keep the first colour for the whole line.
		Line Parse(std::uint32_t a_id, std::string_view a_raw)
		{
			Line line{ a_id, {}, Clock::now() };
			for (std::size_t i = 0; i < a_raw.size();) {
				if (a_raw[i] != '<') {
					line.text += a_raw[i++];
					continue;
				}
				const auto end = a_raw.find('>', i);
				if (end == std::string_view::npos) {
					line.text += a_raw.substr(i);
					break;
				}
				const auto tag = a_raw.substr(i, end - i);
				if (const auto hash = tag.find('#'); line.colour < 0 && hash != std::string_view::npos && tag.find("color") != std::string_view::npos && hash + 7 <= tag.size()) {
					int value = 0;
					if (std::from_chars(tag.data() + hash + 1, tag.data() + hash + 7, value, 16).ptr == tag.data() + hash + 7) {
						line.colour = value;
					}
				}
				i = end + 1;
			}
			return line;
		}

		SLSFW_ClaimMessagesFn g_claim{ nullptr };
		SLSFW_GetMessagesFn   g_get{ nullptr };
		bool                  g_claimed{ false };
		std::uint32_t         g_lastId{ 0 };
		std::deque<Line>      g_lines;  // render thread only
		constexpr std::size_t kHistory = 30;

		ImU32 Col(int a_r, int a_g, int a_b, float a_alpha)
		{
			return IM_COL32(a_r, a_g, a_b, static_cast<int>(std::clamp(a_alpha, 0.0f, 1.0f) * 255.0f));
		}

		ImVec2 Measure(ImGuiMCP::ImFont* a_font, float a_size, float a_wrap, const char* a_text)
		{
			return ImGuiMCP::ImFontManger::CalcTextSizeA(a_font, a_size, FLT_MAX, a_wrap, a_text, nullptr, nullptr);
		}

		void Poll()
		{
			if (!g_get) {
				return;
			}
			std::array<SLSFW_Message, 16> batch{};
			for (;;) {
				const auto count = g_get(batch.data(), static_cast<std::uint32_t>(batch.size()), g_lastId);
				for (std::uint32_t i = 0; i < count; ++i) {
					g_lastId = batch[i].id;
					g_lines.push_back(Parse(batch[i].id, batch[i].text));
				}
				while (g_lines.size() > kHistory) {
					g_lines.pop_front();
				}
				if (count < batch.size()) {
					break;
				}
			}
		}

		std::vector<Line> Demo()
		{
			const auto now = Clock::now();
			return {
				{ 0, "You don't have a licence to carry a weapon here.", now },
				{ 0, "The toll is 100 gold. The guards won't let you leave without paying.", now },
				{ 0, "Curfew begins at 21:00. Find an inn or a home.", now },
			};
		}

		void __stdcall Draw()
		{
			const auto& s = Settings::Get();
			auto*       io = ImGuiMCP::GetIO();
			auto*       ui = RE::UI::GetSingleton();
			if (!io || !ui || !g_claimed) {
				return;
			}
			Poll();

			Context context;
			(void)Meters::Read(context);
			const bool preview = Hud::MenuOpen() && Hud::PreviewEnabled();
			if (!preview && (!context.ready || context.menu || !ui->IsShowingMenus())) {
				return;
			}

			// which lines, and how visible: new ones fade in, old ones fade out; the expand key shows the history
			const auto  now = Clock::now();
			const float life = std::max(2.0f, s.fMsgSeconds);
			const bool  history = Hud::Expanded();
			const auto  maxLines = static_cast<std::size_t>(std::clamp(history ? 12 : s.iMsgLines, 1, 30));
			std::vector<std::pair<const Line*, float>> shown;
			const auto demo = preview && g_lines.empty() ? Demo() : std::vector<Line>{};
			std::vector<const Line*> source;
			for (const auto& line : demo) {
				source.push_back(&line);
			}
			for (const auto& line : g_lines) {
				source.push_back(&line);
			}
			for (auto it = source.rbegin(); it != source.rend() && shown.size() < maxLines; ++it) {
				const float age = std::chrono::duration<float>(now - (*it)->arrived).count();
				float       alpha = 1.0f;
				if (!history && !preview) {
					if (age >= life) {
						continue;
					}
					alpha = std::min(std::clamp(age / 0.25f, 0.0f, 1.0f), std::clamp((life - age) / 1.5f, 0.0f, 1.0f));
				}
				shown.emplace_back(*it, alpha);
			}
			if (shown.empty()) {
				return;
			}
			std::reverse(shown.begin(), shown.end());  // oldest at the top, newest at the bottom

			const float k = std::clamp(s.fScale, 0.4f, 3.0f) * std::clamp(s.fMsgScale, 0.5f, 3.0f) * (io->DisplaySize.y / 1080.0f);
			const float pad = 8.0f * k;
			const float size = 15.0f * k;
			const float width = std::clamp(s.fMsgWidth, 200.0f, 900.0f) * k;
			const float wrap = width - pad * 2.0f;
			auto*       font = Typo::Font();
			float       height = pad * 2.0f;
			std::vector<float> heights;
			for (const auto& [line, alpha] : shown) {
				heights.push_back(Measure(font, size, wrap, line->text.c_str()).y);
				height += heights.back() + 4.0f * k;
			}
			height -= 4.0f * k;

			float x = std::clamp(s.fMsgPosX, 0.0f, 1.0f) * io->DisplaySize.x;
			float y = std::clamp(s.fMsgPosY, 0.0f, 1.0f) * io->DisplaySize.y;

			// in preview the feed can be dragged like the panel
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
						edit.fMsgPosX = std::clamp((io->MousePos.x - grab.x) / io->DisplaySize.x, 0.0f, 1.0f);
						edit.fMsgPosY = std::clamp((io->MousePos.y - grab.y) / io->DisplaySize.y, 0.0f, 1.0f);
					} else {
						dragging = false;
						Settings::Save();
					}
				}
			} else {
				dragging = false;
			}

			float maxAlpha = 0.0f;
			for (const auto& entry : shown) {
				maxAlpha = std::max(maxAlpha, entry.second);
			}
			const float opacity = std::clamp(s.fOpacity, 0.1f, 1.0f);
			auto*       list = ImGuiMCP::GetBackgroundDrawList();  // under every ImGui window, so the SMF menu covers it
			const float bg = std::clamp(s.fMsgBackground, 0.0f, 0.85f);  // the preview shows it as set; its frame marks the drag area
			if (bg > 0.005f) {
				DL::AddRectFilled(list, { x, y }, { x + width, y + height }, Col(8, 10, 16, bg * opacity * maxAlpha), 8.0f * k, ImGuiMCP::ImDrawFlags_RoundCornersAll);
			}
			// a thin accent on the left marks the panel as SL Survival's
			DL::AddRectFilled(list, { x, y + 4.0f * k }, { x + 3.0f * k, y + height - 4.0f * k }, Col(232, 176, 92, 0.85f * opacity * maxAlpha), 1.5f * k,
				ImGuiMCP::ImDrawFlags_RoundCornersAll);
			if (preview) {
				DL::AddRect(list, { x, y }, { x + width, y + height }, Col(255, 255, 255, dragging ? 0.7f : 0.3f), 8.0f * k, ImGuiMCP::ImDrawFlags_RoundCornersAll, std::max(1.0f, k));
			}

			float cursor = y + pad;
			for (std::size_t i = 0; i < shown.size(); ++i) {
				const auto& [line, alpha] = shown[i];
				const float a = alpha * opacity;
				const bool  newest = i + 1 == shown.size();
				const int  c = line->colour;
				const auto colour = c >= 0 ? Col((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, (newest ? 1.0f : 0.85f) * a) :
				                             (newest ? Col(255, 236, 200, a) : Col(236, 238, 242, 0.85f * a));
				Typo::Draw(list, font, size, { x + pad, cursor }, colour, line->text.c_str(), wrap, 0.7f * a);
				cursor += heights[i] + 4.0f * k;
			}
		}
	}

	void Init()
	{
		if (const auto module = ::GetModuleHandleW(L"SLSurvivalFramework.dll")) {
			g_claim = reinterpret_cast<SLSFW_ClaimMessagesFn>(::GetProcAddress(module, "SLSFW_ClaimMessages"));
			g_get = reinterpret_cast<SLSFW_GetMessagesFn>(::GetProcAddress(module, "SLSFW_GetMessages"));
		}
		if (g_claim && g_get && Settings::Get().bMessages && SKSEMenuFramework::IsInstalled()) {
			g_claimed = g_claim();
		}
		logger::info("SL Survival messages: {}", Describe());
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			return;
		}
		static auto* element = SKSEMenuFramework::AddHudElement(Draw);
		(void)element;
	}

	bool Claimed()
	{
		return g_claimed;
	}

	std::string Describe()
	{
		if (!g_claim || !g_get) {
			return "not found (SexLab Survival Framework 0.6.0+): SL Survival uses the game's notifications";
		}
		return g_claimed ? "shown in this panel (SLSurvivalFramework.dll)" : "off: SL Survival uses the game's notifications";
	}
}
