#pragma once

#include <array>
#include <string>

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

    // numbers: the bundled StratumNo2 when present (digits and Latin only); otherwise Bahnschrift,
    // which also covers Cyrillic player names
    Cs2Text(RadarDrawContext& draw, bool numbers) : draw_(draw), numbers_(numbers) {}

    bool Available(int px, bool bold);
    float Measure(const char* utf8, int px, bool bold);

    // utf8 cut at a character boundary, with an ellipsis, so that it fits max_w
    std::string Fit(const char* utf8, int px, bool bold, float max_w);

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
    bool numbers_;
    std::array<SizedFont, 24> fonts_{};
    int count_{};
};
