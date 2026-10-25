#pragma once

namespace Vitals
{
	enum class MeterId : std::uint8_t
	{
		kWetness,
		kArousal,
		kDirt,
		kWarmth,
		kHunger,
		kFatigue,
		kCycle,
		kCraving,
		kTotal
	};
	inline constexpr std::size_t kMeterCount = static_cast<std::size_t>(MeterId::kTotal);

	// which way the bar should go for the character's sake; colours trend arrows green or red
	enum class Better : std::uint8_t
	{
		kNeither,
		kUp,
		kDown
	};

	struct Snapshot
	{
		bool        available{ false };  // the source mod is installed and reports a value
		bool        nominal{ true };     // nothing worth showing in auto mode
		float       fill{ 0.0f };        // bar, 0..1
		float       severity{ 0.0f };    // 0 fine .. 1 critical: tints the bar
		std::int8_t trend{ 0 };          // -3..3 arrows, sign is the direction of the bar
		bool        burst{ false };      // a sudden jump (bath, meal, orgasm) rather than a steady rate
		const char* icon{ nullptr };     // stage picture: Data/Interface/VitalsHUD/<icon>.png
		std::string stage;               // "Chilly", "Ovulation"...
		std::string value;               // "34%", "2nd trimester"...
		std::string badge;               // short extra tag ("Addict", "Fertile")
		std::array<float, 6> marks{};    // stage boundaries on the bar, 0..1
		std::uint8_t         markCount{ 0 };
	};

	struct MeterInfo
	{
		const char* key;    // ini key suffix and log name
		const char* title;  // menu label
		const char* source; // where the value comes from
		const char* glyph;  // FontAwesome solid glyph, UTF-8: fallback when a stage picture fails to load
		std::uint32_t accent; // IM_COL32 packed (r, g, b)
		Better        better;
	};

	const MeterInfo& Info(MeterId a_id);
	std::int32_t&    Visibility(MeterId a_id);  // the meter's 0 auto / 1 always / 2 never setting

	// player situation, sampled with the meters on the main thread
	struct Context
	{
		bool ready{ false };
		bool combat{ false };
		bool scene{ false };     // in a SexLab animation
		bool dialogue{ false };
		bool menu{ false };      // a full-screen game menu is open (inventory, map, barter, loading...)
	};

	namespace Meters
	{
		void Init();       // kDataLoaded: resolve forms and DLL exports
		void Start();      // the sampling timer (once)
		void OnGameLoaded();
		void SetReady(bool a_ready);

		// render thread: copy of the last published state
		std::array<Snapshot, kMeterCount> Read(Context& a_context);

		// main thread: a full-screen game menu is open. UI::GameIsPaused() cannot be used: with Skyrim Souls RE
		// and always-open menus (SkyParkour, TrueHUD) the pause counter never returns to zero
		bool GameMenuOpen();

		// menu: one line per meter describing the source that was found
		std::string Describe(MeterId a_id);
	}
}
