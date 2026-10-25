#pragma once

namespace Vitals::Hud
{
	// The panel, drawn by SKSE Menu Framework every frame. Needs the framework; does nothing without it.
	void Register();

	// compact (pictures + arrows) <-> detailed panel; the expand key calls this
	void ToggleExpanded();
	bool Expanded();

	// while the framework's menu is open, show every meter (demo values for missing ones) and allow dragging
	bool PreviewEnabled();
	bool MenuOpen();  // the framework's menu is open
	void SetPreview(bool a_enabled);
}
