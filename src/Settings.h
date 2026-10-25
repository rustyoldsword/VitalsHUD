#pragma once

namespace Vitals
{
	// X(section, key, default). Keys match Data/SKSE/Plugins/VitalsHUD.ini.
#define VITALS_FLOAT_SETTINGS(X)              \
	X(Panel, fPosX, 0.989453f)                \
	X(Panel, fPosY, 0.05625f)                 \
	X(Panel, fScale, 1.25f)                   \
	X(Panel, fOpacity, 1.0f)                  \
	X(Panel, fBackground, 0.0f)               \
	X(Panel, fBarWidth, 150.0f)               \
	X(Panel, fIconSize, 1.4f)                 \
	X(Panel, fIconPlate, 0.75f)               \
	X(Text, fTextWeight, 0.7f)                \
	X(Text, fOutline, 0.3f)                   \
	X(Trend, fSensitivity, 1.0f)              \
	X(Visibility, fLinger, 8.0f)                \
	X(Messages, fMsgPosX, 0.015781f)          \
	X(Messages, fMsgPosY, 0.270833f)          \
	X(Messages, fMsgWidth, 359.0f)            \
	X(Messages, fMsgScale, 1.3f)              \
	X(Messages, fMsgBackground, 0.0f)         \
	X(Messages, fMsgSeconds, 10.0f)

#define VITALS_BOOL_SETTINGS(X)               \
	X(Panel, bEnabled, true)                  \
	X(Panel, bHorizontal, false)              \
	X(Panel, bShowLabels, true)               \
	X(Panel, bShowValues, true)               \
	X(Panel, bAlignRight, true)               \
	X(Panel, bCompact, true)                  \
	X(Visibility, bHideInCombat, false)       \
	X(Visibility, bHideInScenes, true)        \
	X(Visibility, bHideInDialogue, false)     \
	X(Messages, bMessages, true)              \
	X(Diseases, bDiseaseColors, true)         \
	X(Diseases, bDiseaseUnknown, true)        \
	X(Diseases, bDiseasePulse, true)

	// 0 auto (only while it matters), 1 always, 2 never. iExpandKey: DirectInput scan code (60 = F2), 0 = none
#define VITALS_INT_SETTINGS(X)                \
	X(Meters, iWetness, 0)                    \
	X(Meters, iArousal, 0)                    \
	X(Meters, iDirt, 0)                       \
	X(Meters, iWarmth, 0)                     \
	X(Meters, iHunger, 0)                     \
	X(Meters, iFatigue, 0)                    \
	X(Meters, iCycle, 0)                      \
	X(Meters, iCraving, 0)                    \
	X(Meters, iSls, 0)                        \
	X(Meters, iDiseases, 0)                   \
	X(Panel, iExpandKey, 60)                  \
	X(Messages, iMsgLines, 5)                 \
	X(General, iLogLevel, 0)

	// [Diseases] sDiseaseHide: comma-separated disease ids switched off in the menu
	// [Text] sFont: a .ttf/.otf in Data/SKSE/Plugins/Fonts (loaded by SKSE Menu Framework), empty = the game font;
	// sOutlineColor: RRGGBBAA
#define VITALS_STRING_SETTINGS(X)             \
	X(Diseases, sDiseaseHide, std::string{})  \
	X(Text, sFont, std::string{ "Rajdhani-SemiBold.ttf" }) \
	X(Text, sOutlineColor, std::string{ "000000D9" })

	struct Settings
	{
#define VITALS_DECLARE(a_section, a_key, a_default) decltype(a_default) a_key{ a_default };
		VITALS_FLOAT_SETTINGS(VITALS_DECLARE)
		VITALS_BOOL_SETTINGS(VITALS_DECLARE)
		VITALS_INT_SETTINGS(VITALS_DECLARE)
		VITALS_STRING_SETTINGS(VITALS_DECLARE)
#undef VITALS_DECLARE

		static Settings& Get();
		static void      Load();
		static bool      Save();
		static void      Reset();

		bool Set(std::string_view a_section, std::string_view a_key, const std::string& a_value);
	};
}
