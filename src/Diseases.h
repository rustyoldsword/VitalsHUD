#pragma once

// Disease block: one icon per disease active on the player. Detection, definition files and icons come from
// Disease Widget - SKSE by kuroishitam (Nexus 195393), integrated with the author's permission.
namespace Vitals::Diseases
{
	struct Active
	{
		std::string id;     // definition id, or "form:XXXXXXXX" for a Disease-type spell without a definition
		std::string name;   // display name ("Ataxia")
		std::string stage;  // spell name when it differs from the display name ("Mild Ataxia"), else empty
		std::string icon;   // icon file stem
		std::array<float, 4> tint{ 0.93f, 0.89f, 0.80f, 1.0f };
		bool        hasTint{ false };
		bool        known{ true };  // false: generic icon for an unrecognised Disease-type spell
		double      firstSeen{ 0.0 };  // NowSeconds()
	};

	struct Definition
	{
		std::string              id;
		std::string              displayName;
		std::string              icon;
		std::string              source;  // definition file(s) the entry came from
		std::vector<std::string> names;   // lower-case spell names
		std::vector<std::string> forms;   // "Plugin.esp|0x1234" as written
		std::vector<RE::FormID>  resolvedForms;
		bool                     requireDiseaseType{ false };
		bool                     hasTint{ false };
		std::array<float, 4>     tint{ 0.93f, 0.89f, 0.80f, 1.0f };
	};
	using DefinitionList = std::vector<Definition>;

	double NowSeconds();  // monotonic, any thread

	void LoadDefinitions();  // kDataLoaded, and the menu's Reload button
	std::shared_ptr<const DefinitionList> Definitions();

	void Sample();         // main thread, with the meters
	void OnGameLoaded();   // a save or a new game: diseases already present get no "new" pulse
	void Clear();          // loading started

	// render thread: the active diseases minus the ones hidden in the menu ([Diseases] sHide) and, unless
	// bUnknown, the unrecognised ones
	std::vector<Active> Read();
	std::vector<Active> ReadAll();  // menu: everything detected

	bool IsHidden(const std::string& a_id);
	void SetHidden(const std::string& a_id, bool a_hidden);

	std::string Describe();
}
