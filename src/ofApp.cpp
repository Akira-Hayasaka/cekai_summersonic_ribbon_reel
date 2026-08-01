#include "ofApp.h"

#include <algorithm>

void ofApp::setup()
{
	ofSetVerticalSync(true);
	ofSetFrameRate(60);
	ofBackground(ofColor::black);
	
	Globals::proto = std::make_shared<Proto>();
	Globals::ELAPSED_TIME = ofGetElapsedTimef();

	ui = std::make_unique<UI>();
	Globals::timeline = std::make_unique<Timeline>(ui->getSequencer());
	Globals::sequencer = ui->getSequencer();
	visual = std::make_unique<Visual>();
	Globals::renderer = std::make_unique<Renderer>();

	ofAddListener(Sequencer::endReachedEvent, this, &ofApp::onSequencerEndReached);
}

void ofApp::update()
{
	Globals::ELAPSED_TIME = ofGetElapsedTimef();
	Globals::timeline->update();

	if (is_rendering)
	{
		Globals::timeline->step();
		visual->update();

		Globals::renderer->open_screen_fbo();
		visual->draw();
		Globals::renderer->close_screen_fbo();

		const auto req = Globals::renderer->get_next_audio_block_request();
		render_audio_buffer.assign(
			static_cast<size_t>(req.numFrames) * static_cast<size_t>(req.channels),
			0.0f);
		std::vector<float> headliner_audio_buffer(render_audio_buffer.size(), 0.0f);

		auto& headliners = visual->getHeadliners();
		size_t activeHeadlinerCount = 0;
		for (const auto& headliner : headliners)
		{
			if (headliner && headliner->isActive() && headliner->hasAudio())
			{
				++activeHeadlinerCount;
			}
		}

		if (activeHeadlinerCount > 0)
		{
			const float mixGain = 1.0f / static_cast<float>(activeHeadlinerCount);
			for (auto& headliner : headliners)
			{
				if (!headliner || !headliner->isActive() || !headliner->hasAudio())
				{
					continue;
				}

				headliner->renderAudioForOfflineRender(
					headliner_audio_buffer.data(),
					req.numFrames,
					req.channels,
					static_cast<double>(req.sampleRate));

				for (size_t i = 0; i < render_audio_buffer.size(); ++i)
				{
					render_audio_buffer[i] += headliner_audio_buffer[i] * mixGain;
				}
			}
		}

		Globals::renderer->feed_to_pipe(
			render_audio_buffer.empty() ? nullptr : render_audio_buffer.data(),
			req.numFrames);
	}
	else
	{
		visual->update();

		Globals::renderer->open_screen_fbo();
		visual->draw();
		Globals::renderer->close_screen_fbo();
	}
}

void ofApp::draw()
{
	ui->draw();
	Globals::timeline->draw();

	Globals::renderer->draw();

	ofDrawBitmapStringHighlight("fps: " + std::to_string(ofGetFrameRate()), 10, ofGetHeight() - 10);
}

void ofApp::keyPressed(int key)
{
	if (key == 'r')
	{
		if (!is_rendering)
		{
			startRendering();
		}
		else
		{
			stopRendering();
		}
	}
}

void ofApp::startRendering()
{
	is_rendering = true;

	Globals::renderer->begin_rendering(ui->getSequencer()->get_fps(), true, RENDER_AUDIO_SAMPLE_RATE, RENDER_AUDIO_CHANNELS);
	for (auto& headliner : visual->getHeadliners())
	{
		if (headliner)
		{
			headliner->setOfflineRenderMode(true);
		}
	}
	ofLogNotice() << "Started rendering.";
}

void ofApp::stopRendering()
{
	if (!is_rendering) return;
	is_rendering = false;

	for (auto& headliner : visual->getHeadliners())
	{
		if (headliner)
		{
			headliner->setOfflineRenderMode(false);
		}
	}

	Globals::renderer->stop_rendering();
	ofLogNotice() << "Stopped rendering.";
}

void ofApp::onSequencerEndReached(SequencerEndReachedEvent& e)
{
	// レンダリング中にシーケンサーが終端フレームに到達したら、レンダリングを停止してアプリを終了する。
	if (!is_rendering) return;

	stopRendering();
	ofExit();
}
void ofApp::keyReleased(int key) { }
void ofApp::mouseMoved(int x, int y) { }
void ofApp::mouseDragged(int x, int y, int button) { }
void ofApp::mousePressed(int x, int y, int button) { }
void ofApp::mouseReleased(int x, int y, int button) { }
void ofApp::mouseEntered(int x, int y) { }
void ofApp::mouseExited(int x, int y) { }
void ofApp::windowResized(int w, int h) { }
void ofApp::gotMessage(ofMessage msg) { }
void ofApp::dragEvent(ofDragInfo dragInfo) { }
