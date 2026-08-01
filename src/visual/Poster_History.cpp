#include "Poster_History.h"

Poster_History::Poster_History(std::vector<std::shared_ptr<Poster>> posters)
    : posters(std::move(posters))
{
}

void Poster_History::update()
{
    for (auto& poster : posters)
    {
        poster->update();
    }
}

void Poster_History::draw()
{
    // w = Constants::app_W, h = Constants::app_H の矩形内に、ポスターをグリッドレイアウトで描画する
    // posterのサイズは600, 841を基準とし、矩形内に収まるように縮小して描画する
    const int numPosters = static_cast<int>(posters.size());
    if (numPosters == 0)
    {
        return;
    }

    constexpr float kPosterBaseWidth = 600.0f;
    constexpr float kPosterBaseHeight = 841.0f;
    const int numColumns = static_cast<int>(std::ceil(std::sqrt(numPosters)));
    const int numRows = static_cast<int>(std::ceil(static_cast<float>(numPosters) / numColumns));
    const float cellWidth = Constants::APP_W / numColumns;
	const float cellHeight = Constants::APP_H / numRows;

    for (int row = 0; row < numRows; ++row)
    {
        for (int col = 0; col < numColumns; ++col)
        {
            const int index = row * numColumns + col;
            if (index >= numPosters)
            {
                break;
            }

            const float scaleToFit = std::min(cellWidth / kPosterBaseWidth, cellHeight / kPosterBaseHeight);
            const float drawScale = std::min(scaleToFit, 1.0f);
            const float drawWidth = kPosterBaseWidth * drawScale;
            const float drawHeight = kPosterBaseHeight * drawScale;

            const float cellX = col * cellWidth;
            const float cellY = row * cellHeight;
            const float x = cellX + (cellWidth - drawWidth) * 0.5f;
            const float y = cellY + (cellHeight - drawHeight) * 0.5f;
            posters[index]->draw(x, y, drawWidth, drawHeight);
        }
    }
}
