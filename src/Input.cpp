#include "Input.h"

#include "Hud.h"
#include "Meters.h"
#include "Settings.h"

#pragma warning(push)
#pragma warning(disable : 4099 5054)
#include <SKSEMenuFramework.h>
#pragma warning(pop)

namespace Vitals::Input
{
	namespace
	{
		// the expand key switches between the compact panel (pictures and arrows) and the detailed one
		class Sink : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				const auto key = static_cast<std::uint32_t>(Settings::Get().iExpandKey);
				if (!a_events || key == 0) {
					return RE::BSEventNotifyControl::kContinue;
				}
				for (auto* event = *a_events; event; event = event->next) {
					auto* button = event->AsButtonEvent();
					if (!button || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard || button->GetIDCode() != key || !button->IsDown()) {
						continue;
					}
					// not while typing in the console or using a menu
					if (Meters::GameMenuOpen()) {
						continue;
					}
					if (SKSEMenuFramework::IsInstalled() && SKSEMenuFramework::IsAnyBlockingWindowOpened()) {
						continue;
					}
					Hud::ToggleExpanded();
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Register()
	{
		static Sink sink;
		if (auto* input = RE::BSInputDeviceManager::GetSingleton()) {
			input->AddEventSink(&sink);
			logger::info("Expand key: scan code {}", Settings::Get().iExpandKey);
		}
	}
}
