#include "Visual.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <vector>

#include "Globals.h"

namespace
{
// タイムコード文字列 -> 秒 の変換で使用するフレームレート。
constexpr float kTimecodeFps = 60.0f;

std::string withExtensionIfNeeded(const std::string& rawPath, const std::string& extension)
{
	if (rawPath.size() >= extension.size() &&
		rawPath.compare(rawPath.size() - extension.size(), extension.size(), extension) == 0)
	{
		return rawPath;
	}
	return rawPath + extension;
}

// "HH:MM:SS:FF" / "MM:SS:FF" / "SS:FF" 形式のタイムコード文字列を秒に変換する。
// fps は FF (フレーム) 部分を秒に変換する際に使用する。
float timecodeToSeconds(const std::string& timecode, float fps)
{
	std::vector<int> parts;
	std::stringstream ss(timecode);
	std::string token;
	while (std::getline(ss, token, ':'))
	{
		try
		{
			parts.push_back(std::stoi(token));
		}
		catch (const std::exception&)
		{
			ofLogWarning() << "Invalid timecode component: " << timecode;
			return 0.0f;
		}
	}

	int hours = 0, minutes = 0, seconds = 0, frames = 0;
	if (parts.size() == 4)
	{
		hours = parts[0]; minutes = parts[1]; seconds = parts[2]; frames = parts[3];
	}
	else if (parts.size() == 3)
	{
		minutes = parts[0]; seconds = parts[1]; frames = parts[2];
	}
	else if (parts.size() == 2)
	{
		seconds = parts[0]; frames = parts[1];
	}
	else
	{
		ofLogWarning() << "Unrecognized timecode format: " << timecode;
		return 0.0f;
	}

	const float framesAsSec = (fps > 0.0f) ? (static_cast<float>(frames) / fps) : 0.0f;
	return static_cast<float>(hours) * 3600.0f + static_cast<float>(minutes) * 60.0f + static_cast<float>(seconds) + framesAsSec;
}

// "offset_sec_from_file_beginning" は小数点秒(number)とタイムコード文字列("HH:MM:SS:FF" 等, 60FPS)の
// どちらでも指定できるようにする。
float parseFileOffsetSec(const nlohmann::json& headlinerConfig)
{
	if (!headlinerConfig.contains("offset_sec_from_file_beginning"))
	{
		return 0.0f;
	}

	const auto& offsetValue = headlinerConfig["offset_sec_from_file_beginning"];
	if (offsetValue.is_string())
	{
		ofLog() << "Parsing offset_sec_from_file_beginning as timecode: " << offsetValue.get<std::string>() << " (fps: " << kTimecodeFps << ") " << timecodeToSeconds(offsetValue.get<std::string>(), kTimecodeFps);
		return timecodeToSeconds(offsetValue.get<std::string>(), kTimecodeFps);
	}
	if (offsetValue.is_number())
	{
		return offsetValue.get<float>();
	}
	return 0.0f;
}

std::unordered_set<std::string> parseTargetYears(const nlohmann::json& packageConfig)
{
	std::unordered_set<std::string> years;

	if (!packageConfig.contains("target_year"))
	{
		return years;
	}

	const auto& targetYear = packageConfig["target_year"];
	if (targetYear.is_number_integer())
	{
		years.insert(std::to_string(targetYear.get<int>()));
		return years;
	}

	if (targetYear.is_array())
	{
		for (const auto& y : targetYear)
		{
			if (y.is_number_integer())
			{
				years.insert(std::to_string(y.get<int>()));
			}
			else if (y.is_string())
			{
				years.insert(y.get<std::string>());
			}
		}
	}

	return years;
}
}

Visual::Visual()
{
	const auto targetYears = parseTargetYears(Globals::package);
	std::vector<int> headlinerYears;

	if (!Globals::asset.empty())
	{
		for (const auto& [year, assetConfig] : Globals::asset.items())
		{
			if (!assetConfig.is_object())
			{
				continue;
			}

			// ポスターのロード。ポスターは全年代ロードする。
			if (assetConfig.contains("poster") && assetConfig["poster"].is_object())
			{
				const auto& posterConfig = assetConfig["poster"];
				const std::string posterPath = posterConfig.value("path", std::string(""));
				if (posterPath.empty())
				{
					continue;
				}

				const std::string imagePath = "assets/" + year + "/" + posterPath;
				ofLogNotice() << "Loading poster image: " << imagePath;
				auto poster = std::make_shared<Poster>(imagePath, year);
				posters.push_back(poster);
			}


			// ヘッドライナーは対象年以外はスキップ
			if (!targetYears.empty() && targetYears.find(year) == targetYears.end())
			{
				continue;
			}

			// ヘッドライナーのロード
			if (assetConfig.contains("headliner") && assetConfig["headliner"].is_object())
			{
				if (!assetConfig["headliner"].empty())
				{
					// Transition が周回する対象年代は、実際にヘッドライナーが読み込まれた年のみとする
					// (target_year が連続していなくても、その間の年を誤って含めないようにするため)
					headlinerYears.push_back(std::stoi(year));
				}

				for (const auto& [headlinerId, headlinerConfig] : assetConfig["headliner"].items())
				{
					(void)headlinerId;
					if (!headlinerConfig.is_object())
					{
						continue;
					}

					const std::string artistName = headlinerConfig.value("name", "");
					ofLog() << "Loading headliner for year " << year << ": " << artistName;
					const std::string mediaStem = headlinerConfig.value("path", std::string("2003_BLUR"));
					const std::string typeStr = headlinerConfig.value("type", std::string("movie"));
					const bool isImg = (typeStr == "img");

					const std::string mediaPath = isImg
						? "assets/" + year + "/" + withExtensionIfNeeded(mediaStem, ".png")
						: "assets/" + year + "/" + withExtensionIfNeeded(mediaStem, ".gv");
					const std::string audioPath = isImg
						? ""
						: "assets/" + year + "/" + withExtensionIfNeeded(mediaStem, ".wav");
					const Headliner::MediaType mediaType = isImg
						? Headliner::MediaType::Img
						: Headliner::MediaType::Movie;

					auto headliner = std::make_shared<Headliner>(std::stoi(year), artistName, mediaPath, audioPath, mediaType);
					headliner->setActivationWindow(
						headlinerConfig.value("from_sec", 0.0f),
						headlinerConfig.value("duration", std::numeric_limits<float>::max()));
					if (!isImg)
					{
						headliner->setFileOffsetFromBeginning(
							parseFileOffsetSec(headlinerConfig));
					}
					headliner->setScale(
						headlinerConfig.value("draw_scale", headlinerConfig.value("scale", 1.0f)));

					if (headlinerConfig.contains("subsection") && headlinerConfig["subsection"].is_array())
					{
						const auto& subsection = headlinerConfig["subsection"];
						if (subsection.size() >= 4 &&
							subsection[0].is_number() && subsection[1].is_number() &&
							subsection[2].is_number() && subsection[3].is_number())
						{
							headliner->setSubsection(
								subsection[0].get<float>(),
								subsection[1].get<float>(),
								subsection[2].get<float>(),
								subsection[3].get<float>());
						}
					}

					if (headlinerConfig.contains("artistname_viewpadding") &&
						headlinerConfig["artistname_viewpadding"].is_object())
					{
						const auto& viewPaddingConfig = headlinerConfig["artistname_viewpadding"];
						headliner->setArtistNameViewPadding(
							viewPaddingConfig.value("left", 80.0f),
							viewPaddingConfig.value("bottom", 70.0f));
					}

					headliners.push_back(std::move(headliner));
				}
			}
		}
	}

	reel = std::make_unique<Reel>(posters, headliners);

	// Transition には、実際にヘッドライナーが読み込まれた年の一覧(ソート・重複排除済み)を渡す。
	// begin/end の連続範囲ではなく、この一覧に基づいて周回することで、target_year に含まれない
	// 年(例: 2020, 2021)が誤って対象に含まれることを防ぐ。
	std::sort(headlinerYears.begin(), headlinerYears.end());
	headlinerYears.erase(std::unique(headlinerYears.begin(), headlinerYears.end()), headlinerYears.end());


	ofAddListener(Globals::sequencer->keyframeEvent, this, &Visual::on_SequencerKeyframeEvent);
}

void Visual::on_SequencerKeyframeEvent(SequencerKeyframeEvent & e) {
	if (e.trackName == "HeadlinerReel") {
		if (e.keyframeName == "in") {
			reel->setHeadlinerDrawEnabled(true);
			for (auto & headliner : headliners)
			{
				if (headliner)
				{
					headliner->setAudioEnabled(false);
				}
			}
		}
	}
	if (e.trackName == "Transition") {
		if (e.keyframeName == "in") {
			reel->setHeadlinerDrawEnabled(false);
			for (auto & headliner : headliners)
			{
				if (headliner)
				{
					headliner->setAudioEnabled(true);
				}
			}
		}
	}
	if (e.trackName == "InfoText") {

	}
	if (e.trackName == "BG") {

	}
	if (e.trackName == "Visual") {
		if (e.keyframeName == "make_reel_front") {
			b_make_reel_front = true;
		}
	}
}

void Visual::update()
{
	for (auto& headliner : headliners)
	{
		headliner->update();
	}

	reel->update();
}

void Visual::draw()
{
	reel->draw();
}

std::vector<std::shared_ptr<Headliner>>& Visual::getHeadliners()
{
	return headliners;
}

const std::vector<std::shared_ptr<Headliner>>& Visual::getHeadliners() const
{
	return headliners;
}

Headliner* Visual::getHeadliner()
{
	if (headliners.empty())
	{
		return nullptr;
	}
	return headliners.front().get();
}

const Headliner* Visual::getHeadliner() const
{
	if (headliners.empty())
	{
		return nullptr;
	}
	return headliners.front().get();
}
