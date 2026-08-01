#pragma once
#include "ofMain.h"
#include "Poster.h"

class Poster_History
{
public:
    Poster_History(std::vector<std::shared_ptr<Poster>> posters);

    void update();
    void draw();

private:

	std::vector<std::shared_ptr<Poster>> posters;

};
