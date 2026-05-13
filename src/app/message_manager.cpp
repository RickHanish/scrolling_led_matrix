#include "app/message_manager.h"

#include <time.h>

#include "app/app_state.h"
#include "config/constants.h"
#include "network/weather.h"
#include "text/text_utils.h"

String getTimeString() {
	struct tm timeinfo;
	if (!getLocalTime(&timeinfo)) {
		return String("Time N/A");
	}

	char monthBuf[16];
	strftime(monthBuf, sizeof(monthBuf), "%b", &timeinfo);

	int hour12 = timeinfo.tm_hour % 12;
	if (hour12 == 0) {
		hour12 = 12;
	}

	char buf[64];
	snprintf(
		buf,
		sizeof(buf),
		"%s %d, %d:%02d %s",
		monthBuf,
		timeinfo.tm_mday,
		hour12,
		timeinfo.tm_min,
		(timeinfo.tm_hour < 12) ? "AM" : "PM"
	);

	return String(buf);
}

void updateMessageTimeout(AppState& state) {
	if (!state.customMessage || state.customMessageStartedAt == 0) {
		return;
	}

	unsigned long customAge = millis() - state.customMessageStartedAt;
	if (customAge >= CUSTOM_MESSAGE_MAX_AGE_MS) {
		state.customMessage = false;
		state.customMessageStartedAt = 0;
		state.defaultIndex = 0;
		state.currentDefaultDisplay = "";
		state.scrollX = WIDTH;
	}
}

String getDisplayMessage(AppState& state) {
	if (state.customMessage && state.message.length() > 0) {
		return state.message;
	}

	if (state.currentDefaultDisplay.length() == 0) {
		if (state.defaultIndex == 0) {
			state.currentDefaultDisplay = String("Welcome!");
		} else if (state.defaultIndex == 1) {
			state.currentDefaultDisplay = getTimeString();
		} else if (state.defaultIndex == 2) {
			state.cachedWeather = getWeatherString();
			state.currentDefaultDisplay = state.cachedWeather;
		} else if (state.defaultIndex == 3) {
			state.currentDefaultDisplay = state.httpServerAddr.length() > 0 ? state.httpServerAddr : String("IP N/A");
		}
	}

	return state.currentDefaultDisplay;
}

void advanceMessageScrollState(AppState& state, const String& displayMessage) {
	if (state.scrollX < -renderedWidth(displayMessage)) {
		state.currentDefaultDisplay = "";
		state.defaultIndex = (state.defaultIndex + 1) % 4;
		state.scrollX = WIDTH;
	}
}
