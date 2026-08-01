#pragma once
#include "Sequencer.h"
#include "imgui_neo_sequencer.h"
#include "ofMain.h"
#include "ofxImGui.h"

class UI
{
public:

	UI()
	{
		gui.setup(nullptr, true, ImGuiConfigFlags_ViewportsEnable, true);
		sequencer = std::make_shared<Sequencer>();
		sequencer->setEmitEventsDuringScrub(true);
	}

	void draw()
	{
		gui.begin();
		sequencer->draw();
		gui.end();
	}

	std::shared_ptr<Sequencer> getSequencer() { return sequencer; }

private:

	ofxImGui::Gui gui;
	std::shared_ptr<Sequencer> sequencer;
};
