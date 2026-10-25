#pragma once

namespace Vitals
{
	// Rate of change in "fractions of the bar per game hour", fitted over a window of game time. Mods update
	// their values in steps (Bathing in Skyrim once per game hour, Survival Mode on its own ticks), so a plain
	// last-two-samples difference would flash between zero and a spike; a least-squares slope over a window
	// longer than the step reads the real pace. Sudden large jumps (a bath, a meal, a night's sleep in one
	// go) are reported as a short burst instead and restart the window, so they do not smear the steady rate.
	class Trend
	{
	public:
		struct Tuning
		{
			float window{ 1.0f };   // game hours fitted
			float slow{ 0.01f };    // fraction per hour for one arrow; x4 for two, x12 for three
			float jump{ 0.08f };    // a single change this large within kJumpHours is a burst
		};

		void Reset()
		{
			_samples.clear();
			_level = 0;
			_burstUntil = {};
		}

		// a_fraction: the bar position 0..1
		void Add(double a_hours, float a_fraction, const Tuning& a_tuning)
		{
			if (!_samples.empty()) {
				const auto& last = _samples.back();
				if (a_hours < last.hours - 1e-4) {
					Reset();  // the clock went backwards: another save
				} else if (a_hours - last.hours < 1e-5) {
					if (std::abs(a_fraction - last.value) < 1e-6f) {
						return;  // paused or a menu: nothing new
					}
					// value moved without game time passing (a scripted change in a menu-paused frame)
				}
			}
			if (!_samples.empty()) {
				const auto& last = _samples.back();
				const float delta = a_fraction - last.value;
				if (std::abs(delta) >= a_tuning.jump && a_hours - last.hours <= kJumpHours) {
					_burstSign = delta > 0.0f ? 1 : -1;
					_burstUntil = std::chrono::steady_clock::now() + kBurstTime;
					_samples.clear();
				}
			}
			_samples.push_back({ a_hours, a_fraction });
			// keep one sample older than the window so the fit always spans it once it is long enough
			while (_samples.size() > 2 && _samples[1].hours < a_hours - a_tuning.window) {
				_samples.pop_front();
			}
			if (_samples.size() > kMaxSamples) {
				_samples.pop_front();
			}
		}

		// slope in fractions per game hour; 0 until the samples span enough time to mean anything
		float Slope(const Tuning& a_tuning) const
		{
			if (_samples.size() < 2) {
				return 0.0f;
			}
			const double span = _samples.back().hours - _samples.front().hours;
			if (span < std::min(0.05, a_tuning.window * 0.25)) {
				return 0.0f;
			}
			double meanX = 0.0, meanY = 0.0;
			for (const auto& s : _samples) {
				meanX += s.hours;
				meanY += s.value;
			}
			meanX /= _samples.size();
			meanY /= _samples.size();
			double num = 0.0, den = 0.0;
			for (const auto& s : _samples) {
				num += (s.hours - meanX) * (s.value - meanY);
				den += (s.hours - meanX) * (s.hours - meanX);
			}
			return den > 1e-9 ? static_cast<float>(num / den) : 0.0f;
		}

		// -3..3 with hysteresis so a rate sitting on a threshold does not flicker
		std::int8_t Level(float a_slope, const Tuning& a_tuning, float a_sensitivity)
		{
			const float slow = a_tuning.slow / std::max(0.1f, a_sensitivity);
			const float pace = std::abs(a_slope);
			const std::array<float, 3> up{ slow, slow * 4.0f, slow * 12.0f };
			int target = 0;
			for (int i = 0; i < 3; ++i) {
				if (pace >= up[i]) {
					target = i + 1;
				}
			}
			int current = std::abs(_level);
			if (target > current) {
				current = target;
			} else if (target < current && pace < up[current - 1] * 0.7f) {
				current = target;
			}
			_level = static_cast<std::int8_t>(a_slope < 0.0f ? -current : current);
			return _level;
		}

		bool Burst(std::int8_t& a_level) const
		{
			if (std::chrono::steady_clock::now() < _burstUntil) {
				a_level = static_cast<std::int8_t>(3 * _burstSign);
				return true;
			}
			return false;
		}

	private:
		struct Sample
		{
			double hours;
			float  value;
		};

		static constexpr double      kJumpHours = 0.35;
		static constexpr std::size_t kMaxSamples = 4096;
		static constexpr auto        kBurstTime = std::chrono::milliseconds(4500);

		std::deque<Sample>                    _samples;
		std::int8_t                           _level{ 0 };
		int                                   _burstSign{ 0 };
		std::chrono::steady_clock::time_point _burstUntil{};
	};
}
