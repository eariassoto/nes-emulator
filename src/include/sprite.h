// Copyright (c) 2020 Emmanuel Arias
#pragma once
#include <string>
#include <glad/glad.h>

namespace cpuemulator {

class Sprite {
   public:
    Sprite(const std::string& spriteName, unsigned int width,
           unsigned int height, unsigned int cellSize, float posX = 0, float posY = 0);
    ~Sprite();

    void Render();

    void SetPixel(int x, int y, int color);

   private:
    std::string m_Name;
    unsigned int m_Width = 0;
    unsigned int m_Height = 0;

    unsigned int m_CellSizeInPixels = 0;

    float m_TextureWidth = 0;
    float m_TextureHeight = 0;

    float m_PositionX = 0;
    float m_PositionY = 0;
    GLubyte* m_TextureData = nullptr;
    GLuint m_textureId = 0;

    static constexpr GLenum PIXEL_FORMAT = GL_BGRA;
    static constexpr int CHANNEL_COUNT = 4;
};
}  // namespace cpuemulator
