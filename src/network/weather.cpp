#include "network/weather.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>

String getWeatherString() {
	HTTPClient http;
	const char* url = "http://wttr.in/Houston?format=j1";

	http.begin(url);
	int httpCode = http.GET();
	if (httpCode == HTTP_CODE_OK) {
		String payload = http.getString();
		http.end();

		JsonDocument doc;
		DeserializationError error = deserializeJson(doc, payload);
		if (error) {
			return String("Weather N/A");
		}

		String condition = doc["current_condition"][0]["weatherDesc"][0]["value"].as<String>();
		int tempFInt = doc["current_condition"][0]["temp_F"].as<int>();
		String tempFStr = doc["current_condition"][0]["temp_F"].as<String>();

		int maxRainChance = 0;
		JsonArray hourly = doc["weather"][0]["hourly"];
		for (JsonObject hour : hourly) {
			int rainChance = hour["chanceofrain"].as<int>();
			if (rainChance > maxRainChance) {
				maxRainChance = rainChance;
			}
		}

		String emoji;
		String lowerCondition = condition;
		lowerCondition.toLowerCase();

		if (lowerCondition.indexOf("thunder") >= 0 ||
				lowerCondition.indexOf("storm") >= 0 ||
				lowerCondition.indexOf("lightning") >= 0) {
			emoji = String("\U0001F329");
		} else if (lowerCondition.indexOf("snow") >= 0 ||
							 lowerCondition.indexOf("sleet") >= 0 ||
							 lowerCondition.indexOf("hail") >= 0 ||
							 lowerCondition.indexOf("blizzard") >= 0 ||
							 tempFInt < 32) {
			emoji = String("\u2744");
		} else if (lowerCondition.indexOf("fog") >= 0 ||
							 lowerCondition.indexOf("mist") >= 0 ||
							 lowerCondition.indexOf("haze") >= 0 ||
							 lowerCondition.indexOf("smoke") >= 0) {
			emoji = String("\U0001F32B");
		} else if (lowerCondition.indexOf("partly") >= 0 ||
							 lowerCondition.indexOf("sun and cloud") >= 0 ||
							 lowerCondition.indexOf("sunny intervals") >= 0) {
			emoji = String("\u26C5");
		} else if (lowerCondition.indexOf("rain") >= 0 ||
							 lowerCondition.indexOf("drizzle") >= 0 ||
							 lowerCondition.indexOf("shower") >= 0) {
			emoji = String("\U0001F4A7");
		} else if (lowerCondition.indexOf("cloud") >= 0 ||
							 lowerCondition.indexOf("overcast") >= 0) {
			emoji = String("\u2601");
		} else {
			emoji = String("\u2600\uFE0F");
		}

		return condition + " " + emoji + ", " + tempFStr + "°F, " + String(maxRainChance) + "% rain";
	}

	http.end();
	return String("Weather N/A");
}
