#include "Poster.h"

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif
#ifndef GL_TEXTURE_LOD_BIAS
#define GL_TEXTURE_LOD_BIAS 0x8501
#endif

namespace
{
    // ミップマップのトライリニア補間は縮小時に平滑化された下位ミップから
    // サンプリングするため、高解像度の原画を縮小するとボケて見える。
    // 負のLODバイアスでより高解像度(シャープ)なミップを参照させ、
    // 原画のくっきり感を取り戻す。0.0=標準、値を小さくするほどシャープ(要エイリアシング注意)。
    constexpr float kPosterLodBias = -3.0f;
}

Poster::Poster(const std::string& image_path, const std::string& year)
    : year(year)
{
    // ポスター原画は高解像度(約2064x2921)だが、グリッドやトランジションでは
    // 大きく縮小して描画される。ミップマップ無し・矩形テクスチャのままだと
    // 縮小時に深刻なエイリアシングが発生し画質が著しく低下する。
    // そのため GL_TEXTURE_2D + ミップマップ + 異方性フィルタ + LODバイアスで
    // 「くっきり」かつ「ジャギーの少ない」縮小描画を両立する。

    // ミップマップは矩形テクスチャ(ARB)では使えないため、GL_TEXTURE_2D で読み込む。
    const bool wasUsingArbTex = ofGetUsingArbTex();
    ofDisableArbTex();

    ofPixels pixels;
    if (ofLoadImage(pixels, image_path))
    {
        image.enableMipmap();
        image.allocate(pixels);
        image.generateMipmap();

        // 縮小: トライリニア(ミップマップ間も線形補間)、拡大: 線形補間。
        image.setTextureMinMagFilter(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR);
        image.setTextureWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        const GLenum target = image.getTextureData().textureTarget;
        image.bind();

        // 異方性フィルタを最大に設定し、斜め方向の縮小でも鮮明さを保つ。
        GLfloat maxAnisotropy = 1.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
        glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);

        // 負のLODバイアスでシャープなミップを参照させ、原画のくっきり感を出す。
        glTexParameterf(target, GL_TEXTURE_LOD_BIAS, kPosterLodBias);

        image.unbind();
    }
    else
    {
        ofLogError("Poster") << "Failed to load poster image: " << image_path;
    }

    if (wasUsingArbTex)
    {
        ofEnableArbTex();
    }
}

void Poster::update()
{
    // Update logic for the poster if needed
}

void Poster::draw(const float x, const float y)
{
    image.draw(x, y);
}

void Poster::draw(const float x, const float y, const float width, const float height)
{
    image.draw(x, y, width, height);
}