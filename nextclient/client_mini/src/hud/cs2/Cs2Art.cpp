#include "Cs2Art.h"

#include <cstdint>
#include <cstdio>
#include <vector>

#include <glad/glad.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
    constexpr const char* kDir = "cstrike/resource/epicentre_hud/";

    // Uncompressed 32-bit TGA, either row order; anything else is treated as missing.
    bool LoadTga(const std::string& path, int& w, int& h, std::vector<uint8_t>& rgba)
    {
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f)
            return false;
        uint8_t header[18];
        bool ok = std::fread(header, 1, sizeof(header), f) == sizeof(header) && header[2] == 2 && header[16] == 32;
        if (ok)
        {
            std::fseek(f, header[0], SEEK_CUR); // image id
            w = header[12] | header[13] << 8;
            h = header[14] | header[15] << 8;
            std::vector<uint8_t> bgra(static_cast<size_t>(w) * h * 4);
            ok = w > 0 && h > 0 && std::fread(bgra.data(), 1, bgra.size(), f) == bgra.size();
            if (ok)
            {
                bool top_left = header[17] & 0x20;
                rgba.resize(bgra.size());
                for (int y = 0; y < h; y++)
                {
                    const uint8_t* src = &bgra[static_cast<size_t>(top_left ? y : h - 1 - y) * w * 4];
                    uint8_t* dst = &rgba[static_cast<size_t>(y) * w * 4];
                    for (int x = 0; x < w; x++)
                    {
                        dst[x * 4 + 0] = src[x * 4 + 2];
                        dst[x * 4 + 1] = src[x * 4 + 1];
                        dst[x * 4 + 2] = src[x * 4 + 0];
                        dst[x * 4 + 3] = src[x * 4 + 3];
                    }
                }
            }
        }
        std::fclose(f);
        return ok;
    }
} // namespace

const char* Cs2Art::NumberFace()
{
    static const char* face = [] () -> const char* {
#ifdef _WIN32
        std::string path = std::string(kDir) + "StratumNo2.ttf";
        wchar_t wide[MAX_PATH];
        if (MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide, MAX_PATH) > 0 &&
            AddFontResourceExW(wide, FR_PRIVATE, nullptr) > 0)
        {
            return "StratumNo2";
        }
#endif
        return nullptr;
    }();
    return face;
}

void Cs2Art::Reset()
{
    // the context that owned the names is gone or about to be: forget them, do not delete
    textures_.clear();
}

unsigned int Cs2Art::Texture(const char* name)
{
    Entry& e = textures_[name];
    if (e.tried)
        return e.texture;
    e.tried = true;

    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
    if (!LoadTga(std::string(kDir) + name + ".tga", w, h, rgba))
        return 0;

    GLint prev = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev);
    glGenTextures(1, &e.texture);
    glBindTexture(GL_TEXTURE_2D, e.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev));
    return e.texture;
}

bool Cs2Art::Has(const char* name)
{
    return Texture(name) != 0;
}

void Cs2Art::Draw(const char* name, float x, float y, float w, float h, ncl_math::Color tint)
{
    unsigned int tex = Texture(name);
    if (!tex)
        return;

    GLint prev = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(tint.r, tint.g, tint.b, tint.a);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(x, y);
    glTexCoord2f(1, 0);
    glVertex2f(x + w, y);
    glTexCoord2f(1, 1);
    glVertex2f(x + w, y + h);
    glTexCoord2f(0, 1);
    glVertex2f(x, y + h);
    glEnd();
    // restore rather than unbind: the engine keeps a bind cache
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev));
    glColor4ub(255, 255, 255, 255);
}
