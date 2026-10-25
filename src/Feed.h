#pragma once

// SL Survival's messages in their own HUD panel instead of the vanilla notification waterfall. SexLab Survival
// Framework 0.6.0+ routes SLS's Debug.Notification calls here once this panel claims them (SLSFW_ClaimMessages).
namespace Vitals::Feed
{
	void        Init();      // kDataLoaded: claims SLS's messages when [Messages] bMessages is on
	void        Register();  // the HUD element (after SKSE Menu Framework is up)
	bool        Claimed();
	std::string Describe();
}
