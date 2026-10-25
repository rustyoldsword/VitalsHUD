#include "Sls.h"

#include "SLSFW_HudStatus.h"
#include "Settings.h"

#include <Windows.h>

namespace Vitals::Sls
{
	namespace
	{
		constexpr const char* kIdCard = "\xef\x8b\x82";    // f2c2 id-card
		constexpr const char* kCoins = "\xef\x94\x9e";     // f51e coins
		constexpr const char* kLock = "\xef\x80\xa3";      // f023 lock
		constexpr const char* kMoon = "\xef\x86\x86";      // f186 moon
		constexpr const char* kSkull = "\xef\x95\x8c";     // f54c skull
		constexpr const char* kWizard = "\xef\x9b\xa8";    // f6e8 hat-wizard
		constexpr const char* kBattery = "\xef\x89\x83";   // f243 battery-quarter

		constexpr std::array<const char*, SLSFW_LicCount> kLicence{ "Magic", "Weapon", "Armor", "Bikini", "Clothes", "Curfew", "Whore", "Freedom", "Property" };
		constexpr std::array<const char*, SLSFW_LicCount> kShort{ "Mag", "Wpn", "Arm", "Bik", "Clo", "Cur", "Wh", "Fre", "Prop" };
		constexpr std::array<const char*, SLSFW_LicCount> kLicenceIcon{ "lic_magic", "lic_weapon", "lic_armor", "lic_bikini", "lic_clothes", "lic_curfew",
			"lic_whore", "lic_freedom", "lic_property" };
		constexpr std::array<const char*, 5>              kTown{ "Whiterun", "Solitude", "Markarth", "Windhelm", "Riften" };

		SLSFW_GetHudStatusFn g_get{ nullptr };
		std::mutex           g_lock;
		std::vector<Item>    g_items;

		// game days as "3 d 4 h" / "14 h"; short "3d" / "14h"
		std::string Duration(float a_days, bool a_short)
		{
			const int hours = static_cast<int>(std::lround(a_days * 24.0f));
			if (hours >= 24) {
				return a_short ? std::format("{}d", hours / 24) : (hours % 24 ? std::format("{} d {} h", hours / 24, hours % 24) : std::format("{} d", hours / 24));
			}
			return std::format("{}h", std::max(hours, 1));
		}

		std::string Hour(float a_hour)
		{
			const int minutes = static_cast<int>(std::lround(std::fmod(std::max(0.0f, a_hour), 24.0f) * 60.0f));
			return std::format("{:02}:{:02}", minutes / 60 % 24, minutes % 60);
		}

		void Build(const SLSFW_HudStatus& a_s, bool a_always, std::vector<Item>& a_out)
		{
			for (std::size_t i = 0; i < SLSFW_LicCount; ++i) {
				if (a_s.licence[i] < 0) {
					continue;
				}
				Item item{ kIdCard, std::format("{} licence", kLicence[i]) };
				item.icon = kLicenceIcon[i];
				if (a_s.licence[i] > 0) {
					item.value = std::format("valid, {} left", Duration(a_s.licenceDaysLeft[i], false));
					item.tag = std::format("{} {}", kShort[i], Duration(a_s.licenceDaysLeft[i], true));
					item.tone = a_s.licenceDaysLeft[i] < 1.0f ? Tone::kWarn : Tone::kGood;
				} else if (a_s.licenceViolation[i]) {
					// carrying what this licence covers in a toll town: always shown, never greyed
					item.value = "none - you carry contraband";
					item.tag = std::format("{} !", kShort[i]);
					item.tone = Tone::kBad;
				} else {
					item.value = "none";
					item.tag = std::format("{} -", kShort[i]);
					item.tone = Tone::kBad;
					item.dim = true;
					item.detailOnly = !a_always;
				}
				a_out.push_back(std::move(item));
			}

			if (a_s.town >= 0 && a_s.tollEnabled) {
				Item item{ kCoins, std::format("Toll, {}", kTown[a_s.town]) };
				if (a_s.tollPaid) {
					item.value = "paid";
					item.tag = "Toll ok";
					item.tone = Tone::kGood;
					item.icon = "toll_paid";
				} else {
					item.value = std::format("not paid ({:.0f} gold)", a_s.tollCost);
					item.tag = std::format("Toll {:.0f}", a_s.tollCost);
					item.tone = a_s.insideWalls ? Tone::kBad : Tone::kWarn;
					item.icon = "toll_unpaid";
				}
				a_out.push_back(std::move(item));
			}
			if (a_s.grounded) {
				a_out.push_back({ kLock, "Confined to town", "you may not leave", "Grounded", Tone::kBad, "sls_grounded" });
			}

			if (a_s.town >= 0 || a_s.curfewInEffect || a_always) {
				Item item{ kMoon, "Curfew" };
				const auto span = std::format("{}-{}", Hour(a_s.curfewBegin), Hour(a_s.curfewEnd));
				if (a_s.curfewViolation) {
					item.value = std::format("breaking it ({})", span);
					item.tag = "Curfew!";
					item.tone = Tone::kBad;
					item.icon = "curfew_breaking";
				} else if (a_s.curfewInEffect) {
					item.value = std::format("in effect until {}", Hour(a_s.curfewEnd));
					item.tag = "Curfew";
					item.tone = Tone::kWarn;
					item.icon = "curfew_on";
				} else {
					item.value = span;
					item.tag = std::format("{:.0f}-{:.0f}", a_s.curfewBegin, a_s.curfewEnd);
					item.tone = Tone::kNeutral;
					item.icon = "curfew_hours";
				}
				a_out.push_back(std::move(item));
			}

			if (a_s.bikiniCurse || a_always) {
				a_out.push_back({ kSkull, "Bikini curse", a_s.bikiniCurse ? "active" : "none", "Bikini", a_s.bikiniCurse ? Tone::kBad : Tone::kNeutral, "curse_bikini", !a_s.bikiniCurse });
			}
			if (a_s.magicCurse || a_always) {
				a_out.push_back({ kWizard, "Magic curse", a_s.magicCurse ? "active" : "none", "Magic", a_s.magicCurse ? Tone::kBad : Tone::kNeutral, "curse_magic", !a_s.magicCurse });
			}

			if (a_s.fatigueLevel > 0 || a_always) {
				static constexpr std::array<const char*, 4> kLevel{ "rested", "tired", "drained", "exhausted" };
				Item item{ kBattery, "Orgasm fatigue" };
				item.value = std::format("{} ({:.1f} of {:.1f} today)", kLevel[a_s.fatigueLevel], a_s.orgasmCount, a_s.orgasmThreshold);
				item.tag = std::format("Fatigue {}", a_s.fatigueLevel);
				item.tone = a_s.fatigueLevel >= 2 ? Tone::kBad : (a_s.fatigueLevel == 1 ? Tone::kWarn : Tone::kNeutral);
				static constexpr std::array<const char*, 4> kFatigueIcon{ "ofat_1", "ofat_1", "ofat_2", "ofat_3" };
				item.icon = kFatigueIcon[a_s.fatigueLevel];
				item.dim = a_s.fatigueLevel == 0;
				a_out.push_back(std::move(item));
			}
		}
	}

	void Init()
	{
		if (const auto module = ::GetModuleHandleW(L"SLSurvivalFramework.dll")) {
			g_get = reinterpret_cast<SLSFW_GetHudStatusFn>(::GetProcAddress(module, "SLSFW_GetHudStatus"));
		}
		logger::info("SL Survival: {}", Describe());
	}

	void Sample()
	{
		// SLS state moves slowly and is read through Papyrus objects: once a second is plenty (the meters run at 4 Hz)
		static int tick = 0;
		if (tick++ % 4 != 0) {
			return;
		}
		std::vector<Item> items;
		const auto        mode = Settings::Get().iSls;
		SLSFW_HudStatus   status{};
		status.size = sizeof(status);
		if (g_get && mode != 2 && g_get(&status)) {
			Build(status, mode == 1, items);
		}
		std::scoped_lock lock(g_lock);
		g_items = std::move(items);
	}

	std::vector<Item> Read()
	{
		std::scoped_lock lock(g_lock);
		return g_items;
	}

	std::string Describe()
	{
		return g_get ? "SLSurvivalFramework.dll SLSFW_GetHudStatus" : "not found (SexLab Survival Framework 0.6.0+)";
	}
}
