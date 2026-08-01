#pragma once

#include "ofMain.h"
#include "Constants.h"

class ArtistName {
public:
	struct ViewPadding {
		float left = 80.0f;
		float bottom = 70.0f;
	};

	ArtistName(const int year, const std::string & name, const ViewPadding & paddingPx = {})
		: year(year), name(name) {

		const std::string logoFilePath = "assets/" + std::to_string(year) + "/" + name + ".png";

		ofLog() << "Creating ArtistName Instance: " << name << " for year: " << year << ", logo file path: " << logoFilePath;

		ofLoadImage(artistLogo, logoFilePath);

		position = computePosition(paddingPx);
	}

	void draw(float alpha = 255.0f) {
		if (!isVisible) return;
		ofPushStyle();
		ofSetColor(255, 255, 255, static_cast<int>(alpha));
		artistLogo.draw(position.x, position.y);
		ofPopStyle();
	}

	const std::string & getName() const { return name; }

	void pop() {
		isVisible = true;
	}

	void reset_pop() {
		isVisible = false;
	}

private:

	int year;
	std::string name;
	ofTexture artistLogo;
	glm::vec2 position;
	bool isVisible = false;

	glm::vec2 computePosition(const ViewPadding & paddingPx) const {
		const float x = paddingPx.left;
		const float y = static_cast<float>(Constants::APP_H) - paddingPx.bottom - artistLogo.getHeight();
		return glm::vec2(x, y);
	}
};

