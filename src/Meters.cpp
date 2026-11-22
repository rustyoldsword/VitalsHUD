#include "Meters.h"

#include "Settings.h"
#include "Diseases.h"
#include "Sls.h"
#include "Trend.h"

#include <Windows.h>

namespace Vitals
{
	namespace
	{
		// FontAwesome 5/6 solid code points (both ship the same free glyphs at these values)
		constexpr const char* kIconDroplet = "\xef\x81\x83";      // f043 droplet
		constexpr const char* kIconHeart = "\xef\x80\x84";        // f004 heart
		constexpr const char* kIconBath = "\xef\x8b\x8d";         // f2cd bath
		constexpr const char* kIconThermometer = "\xef\x8b\x89";  // f2c9 thermometer-half
		constexpr const char* kIconUtensils = "\xef\x8b\xa7";     // f2e7 utensils
		constexpr const char* kIconBed = "\xef\x88\xb6";          // f236 bed
		constexpr const char* kIconVenus = "\xef\x88\xa1";        // f221 venus
		constexpr const char* kIconFlask = "\xef\x83\x83";        // f0c3 flask

		constexpr std::uint32_t Rgb(int a_r, int a_g, int a_b)
		{
			return static_cast<std::uint32_t>(a_r) | (static_cast<std::uint32_t>(a_g) << 8) | (static_cast<std::uint32_t>(a_b) << 16);
		}

		constexpr std::array<MeterInfo, kMeterCount> kInfo{ {
			{ "Wetness", "Wetness", "Drench / WetFunction NG", kIconDroplet, Rgb(80, 176, 255), Better::kNeither },
			{ "Arousal", "Arousal", "OSL Aroused / SexLab Aroused", kIconHeart, Rgb(255, 92, 150), Better::kNeither },
			{ "Dirt", "Dirt", "Bathing in Skyrim", kIconBath, Rgb(176, 132, 84), Better::kDown },
			{ "Warmth", "Warmth", "Survival Mode", kIconThermometer, Rgb(255, 150, 70), Better::kUp },
			{ "Hunger", "Hunger", "Survival Mode", kIconUtensils, Rgb(232, 196, 92), Better::kDown },
			{ "Fatigue", "Fatigue", "Survival Mode", kIconBed, Rgb(150, 140, 255), Better::kDown },
			{ "Cycle", "Cycle", "Beeing Female", kIconVenus, Rgb(236, 120, 200), Better::kNeither },
			{ "Craving", "Cum craving", "SL Survival", kIconFlask, Rgb(230, 230, 210), Better::kDown },
		} };

		// window (game hours), one-arrow pace (fraction of the bar per game hour), burst jump (fraction)
		constexpr std::array<Trend::Tuning, kMeterCount> kTuning{ {
			{ 0.5f, 0.010f, 0.20f },  // wetness: native rate, window unused
			{ 0.6f, 0.030f, 0.15f },  // arousal: SexLab scenes move it fast
			{ 3.0f, 0.004f, 0.08f },  // dirt: Bathing in Skyrim ticks once per game hour (wilderness 1.5 %/h)
			{ 1.0f, 0.010f, 0.10f },  // warmth: 1000-point cold
			{ 2.0f, 0.0025f, 0.05f }, // hunger: base 7.5 per hour of 1000
			{ 2.0f, 0.003f, 0.06f },  // fatigue: base 9 per hour of 960
			{ 1.0f, 1.0f, 2.0f },     // cycle: progress only
			{ 3.0f, 0.010f, 0.10f },  // craving
		} };

		// ---- sources ---------------------------------------------------------------------------------------

		struct WfngGauge
		{
			std::uint32_t size;
			bool          active;
			float         wetness;
			float         rate;
			float         dry;
			float         sweat;
			float         soaked;
			float         cap;
		};
		using WfngGetGaugeFn = bool (*)(WfngGauge*);
		using OslGetArousalFn = float (*)(RE::Actor*);
		using SlaGetArousalFn = std::int32_t (*)(RE::Actor*);

		struct Sources
		{
			WfngGetGaugeFn  wfng{ nullptr };
			const char*     wetnessSource{ "" };
			SlaGetArousalFn slaNG{ nullptr };
			OslGetArousalFn osl{ nullptr };
			RE::TESFaction* slaFaction{ nullptr };

			RE::TESGlobal* bisDirt{ nullptr };
			std::array<RE::TESGlobal*, 5> bisTiers{};

			RE::TESGlobal* survivalEnabled{ nullptr };
			RE::TESGlobal* survivalCold{ nullptr };
			RE::TESGlobal* survivalHunger{ nullptr };
			RE::TESGlobal* survivalExhaustion{ nullptr };
			RE::TESGlobal* survivalColdMax{ nullptr };
			RE::TESGlobal* survivalHungerMax{ nullptr };
			RE::TESGlobal* survivalExhaustionMax{ nullptr };

			RE::TESGlobal* bfEnabled{ nullptr };
			RE::TESGlobal* bfState{ nullptr };
			RE::TESGlobal* bfPercent{ nullptr };

			RE::TESFaction* sexLabAnimating{ nullptr };

			RE::TESGlobal* slsPool{ nullptr };
			RE::TESGlobal* slsHunger{ nullptr };
			std::array<RE::TESGlobal*, 4> slsHungerSteps{};
		};
		Sources g_src;

		RE::TESGlobal* Global(RE::FormID a_id, std::string_view a_plugin)
		{
			return RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(a_id, a_plugin);
		}

		float Value(const RE::TESGlobal* a_global, float a_fallback = 0.0f)
		{
			return a_global && std::isfinite(a_global->value) ? a_global->value : a_fallback;
		}

		std::string Percent(float a_fraction)
		{
			return std::format("{:.0f}%", std::clamp(a_fraction, 0.0f, 1.0f) * 100.0f);
		}

		// stage index for ascending lower bounds; bounds[0] is the start of stage 1
		template <std::size_t N>
		int StageOf(float a_value, const std::array<float, N>& a_bounds)
		{
			int stage = 0;
			for (std::size_t i = 0; i < N; ++i) {
				if (a_value >= a_bounds[i]) {
					stage = static_cast<int>(i) + 1;
				}
			}
			return stage;
		}

		template <std::size_t N>
		void Marks(Snapshot& a_out, const std::array<float, N>& a_bounds, float a_max, bool a_inverted = false)
		{
			a_out.markCount = 0;
			for (const float bound : a_bounds) {
				if (a_out.markCount >= a_out.marks.size()) {
					break;
				}
				float f = a_max > 0.0f ? bound / a_max : 0.0f;
				if (a_inverted) {
					f = 1.0f - f;
				}
				if (f > 0.005f && f < 0.995f) {
					a_out.marks[a_out.markCount++] = f;
				}
			}
		}

		// ---- the sampler -----------------------------------------------------------------------------------

		struct State
		{
			std::array<Trend, kMeterCount>    trends;
			std::array<Snapshot, kMeterCount> work;
		};

		std::mutex                        g_publishLock;
		std::array<Snapshot, kMeterCount> g_published;
		Context                           g_context;
		State                             g_state;  // main thread only
		std::atomic_bool                  g_ready{ false };
		std::atomic_bool                  g_queued{ false };
		std::atomic_bool                  g_resetTrends{ false };

		void ApplyTrend(MeterId a_id, Snapshot& a_out, double a_hours, float a_slopeOverride = std::numeric_limits<float>::quiet_NaN())
		{
			const auto  index = static_cast<std::size_t>(a_id);
			auto&       trend = g_state.trends[index];
			const auto& tuning = kTuning[index];
			trend.Add(a_hours, a_out.fill, tuning);
			std::int8_t burst = 0;
			if (trend.Burst(burst)) {
				a_out.trend = burst;
				a_out.burst = true;
				return;
			}
			const float slope = std::isnan(a_slopeOverride) ? trend.Slope(tuning) : a_slopeOverride;
			a_out.trend = trend.Level(slope, tuning, Settings::Get().fSensitivity);
		}

		void SampleWetness(Snapshot& a_out, double a_hours)
		{
			WfngGauge gauge{};
			gauge.size = sizeof(gauge);
			if (!g_src.wfng || !g_src.wfng(&gauge) || !gauge.active) {
				return;
			}
			const float cap = std::max(0.1f, gauge.cap);
			a_out.available = true;
			a_out.fill = std::clamp(gauge.wetness / cap, 0.0f, 1.0f);
			const std::array<float, 3> bounds{ gauge.dry, gauge.sweat, gauge.soaked };
			static constexpr std::array<const char*, 4> kNames{ "Dry", "Damp", "Sweaty", "Soaked" };
			static constexpr std::array<const char*, 4> kIcons{ "wet_dry", "wet_damp", "wet_sweaty", "wet_soaked" };
			const int stage = gauge.wetness <= 0.01f ? 0 : StageOf(gauge.wetness, bounds);
			a_out.stage = kNames[stage];
			a_out.icon = kIcons[stage];
			a_out.value = Percent(a_out.fill);
			a_out.severity = 0.0f;
			a_out.nominal = gauge.wetness <= gauge.dry;
			Marks(a_out, bounds, cap);
			// Drench (and WetFunction NG) know their own rate: use it instead of fitting the sampled value
			ApplyTrend(MeterId::kWetness, a_out, a_hours, a_out.fill <= 0.001f && gauge.rate < 0.0f ? 0.0f : gauge.rate / cap);
		}

		std::int32_t ReadArousal(RE::Actor* a_player)
		{
			std::int32_t value = -1;
			if (g_src.slaNG) {
				value = g_src.slaNG(a_player);
			} else if (g_src.osl) {
				const float raw = g_src.osl(a_player);
				value = std::isfinite(raw) ? static_cast<std::int32_t>(std::lround(raw)) : -1;
			} else if (g_src.slaFaction) {
				value = a_player->GetFactionRank(g_src.slaFaction, true);
			}
			// frameworks answer garbage for actors they have not initialised yet
			return value >= 0 && value <= 100 ? value : -1;
		}

		void SampleArousal(Snapshot& a_out, RE::Actor* a_player, double a_hours)
		{
			const auto arousal = ReadArousal(a_player);
			if (arousal < 0) {
				return;
			}
			a_out.available = true;
			a_out.fill = arousal / 100.0f;
			static constexpr std::array<float, 4>       kBounds{ 20.0f, 40.0f, 60.0f, 80.0f };
			static constexpr std::array<const char*, 5> kNames{ "Calm", "Interested", "Aroused", "Lustful", "Desperate" };
			static constexpr std::array<const char*, 5> kIcons{ "aro_calm", "aro_interested", "aro_aroused", "aro_lustful", "aro_desperate" };
			const int stage = StageOf(static_cast<float>(arousal), kBounds);
			a_out.stage = kNames[stage];
			a_out.icon = kIcons[stage];
			a_out.value = std::to_string(arousal);
			a_out.severity = stage >= 4 ? 0.6f : 0.0f;
			a_out.nominal = arousal < 20;
			Marks(a_out, kBounds, 100.0f);
			ApplyTrend(MeterId::kArousal, a_out, a_hours);
		}

		void SampleDirt(Snapshot& a_out, double a_hours)
		{
			if (!g_src.bisDirt) {
				return;
			}
			const float dirt = std::clamp(Value(g_src.bisDirt), 0.0f, 1.0f);
			// Bathing in Skyrim's tiers: the "get ..." thresholds of its MCM (defaults 20/60/98/100 %)
			const std::array<float, 4> bounds{ Value(g_src.bisTiers[1], 0.2f), Value(g_src.bisTiers[2], 0.6f), Value(g_src.bisTiers[3], 0.98f), Value(g_src.bisTiers[4], 1.0f) };
			static constexpr std::array<const char*, 5> kNames{ "Clean", "Not dirty", "Slightly dirty", "Quite dirty", "Filthy" };
			static constexpr std::array<const char*, 5> kIcons{ "dirt_clean", "dirt_light", "dirt_slight", "dirt_quite", "dirt_filthy" };
			const int stage = StageOf(dirt, bounds);
			a_out.icon = kIcons[stage];
			a_out.available = true;
			a_out.fill = dirt;
			a_out.stage = kNames[stage];
			a_out.value = Percent(dirt);
			a_out.severity = std::clamp((stage - 1) / 3.0f, 0.0f, 1.0f);
			a_out.nominal = stage == 0;
			Marks(a_out, bounds, 1.0f);
			ApplyTrend(MeterId::kDirt, a_out, a_hours);
		}

		bool SurvivalOn()
		{
			return g_src.survivalEnabled && Value(g_src.survivalEnabled) >= 1.0f;
		}

		// Survival Mode's own stage values (Survival_NeedCold/Hunger/Exhaustion.SetNeedStageValues)
		void SampleWarmth(Snapshot& a_out, double a_hours)
		{
			if (!SurvivalOn() || !g_src.survivalCold) {
				return;
			}
			const float max = std::max(1.0f, Value(g_src.survivalColdMax, 1000.0f));
			const float cold = std::clamp(Value(g_src.survivalCold), 0.0f, max);
			static constexpr std::array<float, 5>       kBounds{ 50.0f, 120.0f, 300.0f, 500.0f, 800.0f };
			static constexpr std::array<const char*, 6> kNames{ "Warm", "Comfortable", "Chilly", "Very cold", "Freezing", "Numb" };
			static constexpr std::array<const char*, 6> kIcons{ "warm_warm", "warm_comfortable", "warm_chilly", "warm_verycold", "warm_freezing", "warm_numb" };
			const int stage = StageOf(cold, kBounds);
			a_out.icon = kIcons[stage];
			a_out.available = true;
			a_out.fill = 1.0f - cold / max;
			a_out.stage = kNames[stage];
			a_out.value = Percent(a_out.fill);
			a_out.severity = std::clamp((stage - 1) / 4.0f, 0.0f, 1.0f);
			a_out.nominal = stage <= 1;
			Marks(a_out, kBounds, max, true);
			ApplyTrend(MeterId::kWarmth, a_out, a_hours);
		}

		void SampleHunger(Snapshot& a_out, double a_hours)
		{
			if (!SurvivalOn() || !g_src.survivalHunger) {
				return;
			}
			const float max = std::max(1.0f, Value(g_src.survivalHungerMax, 1000.0f));
			const float hunger = std::clamp(Value(g_src.survivalHunger), 0.0f, max);
			static constexpr std::array<float, 5>       kBounds{ 80.0f, 160.0f, 340.0f, 520.0f, 770.0f };
			static constexpr std::array<const char*, 6> kNames{ "Well fed", "Satisfied", "Peckish", "Hungry", "Starving", "Ravenous" };
			static constexpr std::array<const char*, 6> kIcons{ "food_wellfed", "food_satisfied", "food_peckish", "food_hungry", "food_starving", "food_ravenous" };
			const int stage = StageOf(hunger, kBounds);
			a_out.icon = kIcons[stage];
			a_out.available = true;
			a_out.fill = hunger / max;
			a_out.stage = kNames[stage];
			a_out.value = Percent(a_out.fill);
			a_out.severity = std::clamp((stage - 1) / 4.0f, 0.0f, 1.0f);
			a_out.nominal = stage <= 1;
			Marks(a_out, kBounds, max);
			ApplyTrend(MeterId::kHunger, a_out, a_hours);
		}

		void SampleFatigue(Snapshot& a_out, double a_hours)
		{
			if (!SurvivalOn() || !g_src.survivalExhaustion) {
				return;
			}
			const float max = std::max(1.0f, Value(g_src.survivalExhaustionMax, 960.0f));
			const float tired = std::clamp(Value(g_src.survivalExhaustion), 0.0f, max);
			static constexpr std::array<float, 5>       kBounds{ 80.0f, 160.0f, 340.0f, 560.0f, 800.0f };
			static constexpr std::array<const char*, 6> kNames{ "Well rested", "Rested", "Slightly tired", "Tired", "Weary", "Debilitated" };
			static constexpr std::array<const char*, 6> kIcons{ "sleep_wellrested", "sleep_rested", "sleep_slightly", "sleep_tired", "sleep_weary", "sleep_debilitated" };
			const int stage = StageOf(tired, kBounds);
			a_out.icon = kIcons[stage];
			a_out.available = true;
			a_out.fill = tired / max;
			a_out.stage = kNames[stage];
			a_out.value = Percent(a_out.fill);
			a_out.severity = std::clamp((stage - 1) / 4.0f, 0.0f, 1.0f);
			a_out.nominal = stage <= 1;
			Marks(a_out, kBounds, max);
			ApplyTrend(MeterId::kFatigue, a_out, a_hours);
		}

		// Beeing Female publishes the player's state and progress through it (0-100 %) in two globals,
		// updated by FWAbilityBeeingFemale on its own ticks. No rate: the cycle only moves forward.
		void SampleCycle(Snapshot& a_out, RE::Actor* a_player)
		{
			if (!g_src.bfState || !g_src.bfPercent || (g_src.bfEnabled && Value(g_src.bfEnabled) < 1.0f)) {
				return;
			}
			const auto* base = a_player->GetActorBase();
			if (!base || base->GetSex() != RE::SEX::kFemale) {
				return;
			}
			const int   state = static_cast<int>(std::lround(Value(g_src.bfState)));
			const float percent = std::clamp(Value(g_src.bfPercent), 0.0f, 100.0f) / 100.0f;
			a_out.available = true;
			a_out.trend = 0;
			a_out.markCount = 0;
			switch (state) {
			case 0:
				a_out.stage = "Follicular";
				a_out.icon = "bf_follicular";
				a_out.fill = percent;
				a_out.nominal = true;
				break;
			case 1:
				a_out.stage = "Ovulation";
				a_out.icon = "bf_ovulation";
				a_out.badge = "Fertile";
				a_out.fill = percent;
				a_out.severity = 0.5f;
				a_out.nominal = false;
				break;
			case 2:
				a_out.stage = "Luteal";
				a_out.icon = "bf_luteal";
				a_out.fill = percent;
				a_out.nominal = true;
				break;
			case 3:
				a_out.stage = "Menstruation";
				a_out.icon = "bf_menstruation";
				a_out.fill = percent;
				a_out.nominal = false;
				break;
			case 4:
			case 5:
			case 6:
				{
					static constexpr std::array<const char*, 3> kTrimester{ "1st trimester", "2nd trimester", "3rd trimester" };
					static constexpr std::array<const char*, 3> kIcons{ "bf_preg1", "bf_preg2", "bf_preg3" };
					a_out.stage = "Pregnant";
					a_out.icon = kIcons[state - 4];
					a_out.badge = kTrimester[state - 4];
					a_out.fill = (static_cast<float>(state - 4) + percent) / 3.0f;
					a_out.marks[0] = 1.0f / 3.0f;
					a_out.marks[1] = 2.0f / 3.0f;
					a_out.markCount = 2;
					a_out.nominal = false;
				}
				break;
			case 7:
				a_out.stage = "Labor";
				a_out.icon = "bf_labor";
				a_out.fill = 1.0f;
				a_out.severity = 1.0f;
				a_out.nominal = false;
				break;
			case 8:
				a_out.stage = "Recovery";
				a_out.icon = "bf_recovery";
				a_out.fill = percent;
				a_out.nominal = false;
				break;
			case 20:
			case 21:
				a_out.stage = state == 21 ? "Chaurus eggs" : "Pregnant";
				a_out.icon = state == 21 ? "bf_eggs" : "bf_preg2";
				a_out.fill = percent;
				a_out.nominal = false;
				break;
			default:
				a_out.available = false;
				return;
			}
			a_out.value = Percent(a_out.fill);
		}

		// SL Survival cum addiction: the hunger rises per hour and drops when fed; the pool is the addiction tier.
		// Only shown once the player is actually addicted (pool >= 100) or hungry.
		void SampleCraving(Snapshot& a_out, double a_hours)
		{
			if (!g_src.slsPool || !g_src.slsHunger) {
				return;
			}
			const float pool = Value(g_src.slsPool);
			const float hunger = std::max(0.0f, Value(g_src.slsHunger));
			const std::array<float, 4> bounds{ Value(g_src.slsHungerSteps[0], 0.5f), Value(g_src.slsHungerSteps[1], 1.3f), Value(g_src.slsHungerSteps[2], 2.0f), Value(g_src.slsHungerSteps[3], 2.6f) };
			if (pool < 100.0f && hunger < bounds[0]) {
				return;
			}
			const float max = std::max(bounds[3] * 1.25f, hunger);
			static constexpr std::array<const char*, 5> kNames{ "Satisfied", "Peckish", "Hungry", "Starving", "Ravenous" };
			static constexpr std::array<const char*, 5> kAddiction{ "", "Tolerant", "Addict", "Dump", "Junkie" };
			static constexpr std::array<const char*, 5> kIcons{ "crave_satisfied", "crave_peckish", "crave_hungry", "crave_starving", "crave_ravenous" };
			const int stage = StageOf(hunger, bounds);
			a_out.icon = kIcons[stage];
			a_out.available = true;
			a_out.fill = std::clamp(hunger / max, 0.0f, 1.0f);
			a_out.stage = kNames[stage];
			a_out.value = Percent(a_out.fill);
			a_out.badge = kAddiction[std::clamp(static_cast<int>(pool / 100.0f), 0, 4)];
			a_out.severity = std::clamp((stage - 1) / 3.0f, 0.0f, 1.0f);
			a_out.nominal = stage == 0;
			Marks(a_out, bounds, max);
			ApplyTrend(MeterId::kCraving, a_out, a_hours);
		}

		void Sample()
		{
			g_queued = false;
			if (!g_ready) {
				return;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* calendar = RE::Calendar::GetSingleton();
			if (!player || !calendar || !player->Is3DLoaded()) {
				return;
			}
			if (g_resetTrends.exchange(false)) {
				for (auto& trend : g_state.trends) {
					trend.Reset();
				}
			}
			const double hours = static_cast<double>(calendar->GetHoursPassed());

			for (auto& snapshot : g_state.work) {
				snapshot = Snapshot{};
			}
			auto& w = g_state.work;
			SampleWetness(w[static_cast<std::size_t>(MeterId::kWetness)], hours);
			SampleArousal(w[static_cast<std::size_t>(MeterId::kArousal)], player, hours);
			SampleDirt(w[static_cast<std::size_t>(MeterId::kDirt)], hours);
			SampleWarmth(w[static_cast<std::size_t>(MeterId::kWarmth)], hours);
			SampleHunger(w[static_cast<std::size_t>(MeterId::kHunger)], hours);
			SampleFatigue(w[static_cast<std::size_t>(MeterId::kFatigue)], hours);
			SampleCycle(w[static_cast<std::size_t>(MeterId::kCycle)], player);
			SampleCraving(w[static_cast<std::size_t>(MeterId::kCraving)], hours);
			Sls::Sample();
			Diseases::Sample();

			Context context;
			context.ready = true;
			context.combat = player->IsInCombat();
			// SexLab marks a finished scene with rank -1, so a plain faction membership test is not enough
			context.scene = g_src.sexLabAnimating && player->GetFactionRank(g_src.sexLabAnimating, true) >= 0;
			if (auto* ui = RE::UI::GetSingleton()) {
				context.dialogue = ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
			}
			context.menu = Meters::GameMenuOpen();

			std::scoped_lock lock(g_publishLock);
			g_published = w;
			g_context = context;
		}
	}

	const MeterInfo& Info(MeterId a_id)
	{
		return kInfo[static_cast<std::size_t>(a_id)];
	}

	std::int32_t& Visibility(MeterId a_id)
	{
		auto& s = Settings::Get();
		switch (a_id) {
		case MeterId::kWetness:
			return s.iWetness;
		case MeterId::kArousal:
			return s.iArousal;
		case MeterId::kDirt:
			return s.iDirt;
		case MeterId::kWarmth:
			return s.iWarmth;
		case MeterId::kHunger:
			return s.iHunger;
		case MeterId::kFatigue:
			return s.iFatigue;
		case MeterId::kCycle:
			return s.iCycle;
		default:
			return s.iCraving;
		}
	}

	namespace Meters
	{
		void Init()
		{
			// Drench is WetFunction NG's successor: same gauge layout, its own module and export name
			if (const auto module = ::GetModuleHandleW(L"Drench.dll")) {
				g_src.wfng = reinterpret_cast<WfngGetGaugeFn>(::GetProcAddress(module, "Drench_GetPlayerGauge"));
				g_src.wetnessSource = g_src.wfng ? "Drench.dll Drench_GetPlayerGauge" : "";
			}
			if (!g_src.wfng) {
				if (const auto module = ::GetModuleHandleW(L"WetFunctionNG.dll")) {
					g_src.wfng = reinterpret_cast<WfngGetGaugeFn>(::GetProcAddress(module, "WFNG_GetPlayerGauge"));
					g_src.wetnessSource = g_src.wfng ? "WetFunctionNG.dll WFNG_GetPlayerGauge" : "";
					if (!g_src.wfng) {
						logger::warn("WetFunctionNG.dll has no WFNG_GetPlayerGauge export: update WetFunction NG to 1.3.1 or newer for the wetness meter");
					}
				}
			}
			if (const auto module = ::GetModuleHandleW(L"SexlabArousedNG.dll")) {
				g_src.slaNG = reinterpret_cast<SlaGetArousalFn>(::GetProcAddress(module, "SLA_GetArousalInt"));
			}
			if (!g_src.slaNG) {
				if (const auto module = ::GetModuleHandleW(L"OSLAroused.dll")) {
					g_src.osl = reinterpret_cast<OslGetArousalFn>(::GetProcAddress(module, "GetArousalExt"));
				}
			}
			g_src.slaFaction = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESFaction>(0x3FC36, "SexLabAroused.esm"sv);

			constexpr auto bis = "Bathing in Skyrim.esp"sv;
			g_src.bisDirt = Global(0xDA8, bis);  // mzinDirtinessPercentage
			g_src.bisTiers = { Global(0x032, bis), Global(0xDAA, bis), Global(0xDAB, bis), Global(0xDAC, bis), Global(0x06F, bis) };

			constexpr auto survival = "ccqdrsse001-survivalmode.esl"sv;
			g_src.survivalEnabled = Global(0x826, survival);     // Survival_ModeEnabled
			g_src.survivalCold = Global(0x81B, survival);        // Survival_ColdNeedValue
			g_src.survivalHunger = Global(0x81A, survival);      // Survival_HungerNeedValue
			g_src.survivalExhaustion = Global(0x816, survival);  // Survival_ExhaustionNeedValue
			g_src.survivalColdMax = Global(0x84B, survival);
			g_src.survivalHungerMax = Global(0x80C, survival);
			g_src.survivalExhaustionMax = Global(0x84A, survival);

			constexpr auto bf = "BeeingFemale.esm"sv;
			g_src.bfEnabled = Global(0x1260D, bf);  // _BFModEnabled
			g_src.bfState = Global(0x60CC3, bf);    // _BFPlayerState
			g_src.bfPercent = Global(0x60CC4, bf);  // _BFPlayerStatePercent

			g_src.sexLabAnimating = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESFaction>(0xE50F, "SexLab.esm"sv);

			constexpr auto sls = "SL Survival.esp"sv;
			g_src.slsPool = Global(0x97F90, sls);    // _SLS_CumAddictionPool
			g_src.slsHunger = Global(0x97F91, sls);  // _SLS_CumAddictionHunger
			g_src.slsHungerSteps = { Global(0x98FC6, sls), Global(0x98FC7, sls), Global(0x98FC8, sls), Global(0x98FC9, sls) };

			for (std::size_t i = 0; i < kMeterCount; ++i) {
				logger::info("{}: {}", kInfo[i].key, Describe(static_cast<MeterId>(i)));
			}
		}

		void Start()
		{
			static std::once_flag once;
			std::call_once(once, [] {
				std::thread([] {
					for (;;) {
						std::this_thread::sleep_for(std::chrono::milliseconds(250));
						if (g_ready && !g_queued.exchange(true)) {
							SKSE::GetTaskInterface()->AddTask(Sample);
						}
					}
				}).detach();
			});
		}

		void OnGameLoaded()
		{
			g_resetTrends = true;
		}

		void SetReady(bool a_ready)
		{
			g_ready = a_ready;
			if (!a_ready) {
				std::scoped_lock lock(g_publishLock);
				g_published = {};
				g_context = {};
			}
		}

		std::array<Snapshot, kMeterCount> Read(Context& a_context)
		{
			std::scoped_lock lock(g_publishLock);
			a_context = g_context;
			return g_published;
		}

		bool GameMenuOpen()
		{
			auto* ui = RE::UI::GetSingleton();
			if (!ui) {
				return false;
			}
			static const std::array<std::string_view, 24> kMenus{
				RE::InventoryMenu::MENU_NAME, RE::MagicMenu::MENU_NAME, RE::MapMenu::MENU_NAME, RE::JournalMenu::MENU_NAME,
				RE::TweenMenu::MENU_NAME, RE::ContainerMenu::MENU_NAME, RE::BarterMenu::MENU_NAME, RE::GiftMenu::MENU_NAME,
				RE::CraftingMenu::MENU_NAME, RE::LockpickingMenu::MENU_NAME, RE::BookMenu::MENU_NAME, RE::StatsMenu::MENU_NAME,
				RE::LevelUpMenu::MENU_NAME, RE::TrainingMenu::MENU_NAME, RE::SleepWaitMenu::MENU_NAME, RE::RaceSexMenu::MENU_NAME,
				RE::LoadingMenu::MENU_NAME, RE::MainMenu::MENU_NAME, RE::FavoritesMenu::MENU_NAME, RE::MessageBoxMenu::MENU_NAME,
				RE::Console::MENU_NAME, RE::TutorialMenu::MENU_NAME, RE::CreditsMenu::MENU_NAME, RE::LockpickingMenu::MENU_NAME
			};
			return std::any_of(kMenus.begin(), kMenus.end(), [&](std::string_view a_name) { return ui->IsMenuOpen(a_name); });
		}

		std::string Describe(MeterId a_id)
		{
			switch (a_id) {
			case MeterId::kWetness:
				return g_src.wfng ? g_src.wetnessSource : "not found (Drench, or WetFunction NG 1.3.1+)";
			case MeterId::kArousal:
				return g_src.slaNG ? "SexlabArousedNG.dll SLA_GetArousalInt" : (g_src.osl ? "OSLAroused.dll GetArousalExt" : (g_src.slaFaction ? "sla_Arousal faction rank" : "not found"));
			case MeterId::kDirt:
				return g_src.bisDirt ? "Bathing in Skyrim.esp mzinDirtinessPercentage" : "not found (Bathing in Skyrim)";
			case MeterId::kWarmth:
			case MeterId::kHunger:
			case MeterId::kFatigue:
				return g_src.survivalEnabled ? (SurvivalOn() ? "Survival Mode (enabled)" : "Survival Mode (installed, switched off)") : "not found (Survival Mode)";
			case MeterId::kCycle:
				return g_src.bfState ? "BeeingFemale.esm _BFPlayerState" : "not found (Beeing Female)";
			case MeterId::kCraving:
				return g_src.slsPool ? "SL Survival.esp cum addiction" : "not found (SL Survival)";
			default:
				return {};
			}
		}
	}
}
