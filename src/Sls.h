#pragma once

// SL Survival status block: licences, toll, curfew, curses, orgasm fatigue, read through SexLab Survival Framework's
// SLSFW_GetHudStatus export (SLSFW 0.6.0+).
namespace Vitals::Sls
{
	enum class Tone : std::uint8_t
	{
		kNeutral,
		kGood,
		kWarn,
		kBad
	};

	struct Item
	{
		const char* glyph{ nullptr };  // FontAwesome solid, UTF-8
		std::string label;             // detailed panel: "Weapon licence"
		std::string value;             // detailed panel: "valid, 3.2 days"
		std::string tag;               // compact panel: "Wpn 3d"
		Tone        tone{ Tone::kNeutral };
		const char* icon{ nullptr };  // picture Data/Interface/VitalsHUD/<icon>.png; the glyph when it is missing
		bool        dim{ false };     // a licence you do not have: the picture greyed out
		bool        detailOnly{ false };  // only in the detailed panel (licences you do not have, unless "Always")
	};

	void        Init();                // kDataLoaded
	void        Sample();              // main thread, with the meters
	std::vector<Item> Read();         // render thread
	std::string Describe();
}
