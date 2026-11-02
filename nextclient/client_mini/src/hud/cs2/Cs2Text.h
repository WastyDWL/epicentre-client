#pragma once

#include <array>

#include <ncl_math/color.h>
#include <ncl_math/vec3.h>

class RadarDrawContext;

// TTF text for the CS2-style HUD through engine_mini's glyph atlas, drawn as RadarDrawContext
// glyph quads. Bahnschrift (Windows 10+) is closest to CS2's HUD face; Arial covers the rest.
class Cs2Text
{
public:
    enum class Align
    {
        Left,
        Center,
        Right,
    };

    explicit Cs2Text(RadarDrawContext& draw) : draw_(draw) {}

    bool Available(int px, bool bold);
    float Measure(const char* utf8, int px, bool bold);

    // pos.y is the top of the text; returns the advance width.
    float Draw(const char* utf8, ncl_math::Vector2 pos, int px, bool bold, ncl_math::Color color, Align align = Align::Left);

    // Atlas fonts survive a video restart, the scheme does not matter here: only the size cache resets.
    void Reset() { count_ = 0; }

private:
    int ResolveFont(int px, bool bold);

    struct SizedFont
    {
        int px;
        bool bold;
        int font;
    };

    RadarDrawContext& draw_;
    std::array<SizedFont, 24> fonts_{};
    int count_{};
};
