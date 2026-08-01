#pragma once
#include "ofMain.h"
#include "Constants.h"

class Poster
{
public:
    Poster(const std::string& image_path, const std::string& year);

    void update();
    void draw(const float x, const float y);
    void draw(const float x, const float y, const float width, const float height);

	const std::string & getYear() const { return year; }
	std::shared_ptr<ofTexture> getTexture() { return std::make_shared<ofTexture>(image); }

private:

    std::string year;
    ofTexture image;
};
