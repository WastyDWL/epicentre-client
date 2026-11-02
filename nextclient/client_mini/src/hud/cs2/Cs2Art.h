#pragma once

#include <string>
#include <unordered_map>

#include <ncl_math/color.h>

// HUD art from cstrike/resource/epicentre_hud (32-bit TGA, made by tools/import-cs2-hud.py) and its
// StratumNo2 font. Every piece is optional: a missing file leaves the HUD's drawn fallback.
class Cs2Art
{
public:
    // Registers the bundled font with the process (once) and reports its face, or nullptr.
    static const char* NumberFace();

    // Textures belong to a GL context: drop the names when it goes away, reload on demand.
    void Reset();

    bool Has(const char* name);

    // Fixed-function textured quad; the caller flushes its own batched primitives first.
    void Draw(const char* name, float x, float y, float w, float h, ncl_math::Color tint);
    static void DrawTexture(unsigned int texture, float x, float y, float w, float h, ncl_math::Color tint);

private:
    unsigned int Texture(const char* name);

    struct Entry
    {
        unsigned int texture = 0;
        bool tried = false;
    };
    std::unordered_map<std::string, Entry> textures_;
};
