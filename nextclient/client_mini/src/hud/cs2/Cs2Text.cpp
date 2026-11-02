#include "Cs2Text.h"

#include <cmath>
#include <cstdint>

#include <interface.h>
#include <next_engine_mini/engine_mini.h>
#include <strtools.h>
#include <ncl_math/aabb.h>

#include "hud/radar/render/RadarDraw.h"
#include "Cs2Art.h"

namespace
{
    TextRenderInterface* Service()
    {
        static TextRenderInterface* service = nullptr;
        static bool resolved = false;

        if (!resolved)
        {
            resolved = true;
            if (CreateInterfaceFn factory = Sys_GetFactory("next_engine_mini.dll"))
            {
                if (auto* engine = static_cast<EngineMiniInterface*>(factory(ENGINE_MINI_INTERFACE_VERSION, nullptr)))
                    service = engine->GetTextRender();
            }
        }
        return service;
    }

    uint32_t NextCodepoint(const char*& cursor)
    {
        uchar32 cp = 0;
        bool error = false;
        cursor += Q_UTF8ToUChar32(cursor, cp, error);
        if (error)
            return 0xFFFD;
        return cp < 0x20 ? 0x20 : (uint32_t)cp;
    }
} // namespace

int Cs2Text::ResolveFont(int px, bool bold)
{
    for (int i = 0; i < count_; i++)
    {
        if (fonts_[i].px == px && fonts_[i].bold == bold)
            return fonts_[i].font;
    }

    TextRenderInterface* service = Service();
    if (!service || count_ == (int)fonts_.size())
        return kInvalidTextFont;

    // the bundled StratumNo2 (CS2's HUD digits) when present, else Windows' Bahnschrift
    const char* bundled = numbers_ ? Cs2Art::NumberFace() : nullptr;

    TextFontDesc desc{};
    desc.face = bundled ? bundled : "Bahnschrift";
    desc.tall = px;
    desc.weight = bold ? 700 : 500;
    desc.flags = kTextFontAntialias | kTextFontDropShadow;
    desc.format = TextGlyphFormat::Coverage;

    TextFont_t font = service->FindOrCreateFont(desc);
    if (font == kInvalidTextFont)
        return kInvalidTextFont;

    service->AddFallbackFace(font, "Arial");
    fonts_[count_++] = {px, bold, font};
    return font;
}

bool Cs2Text::Available(int px, bool bold)
{
    return draw_.CheckPrimsAvailable() && ResolveFont(px, bold) != kInvalidTextFont;
}

float Cs2Text::Measure(const char* utf8, int px, bool bold)
{
    TextRenderInterface* service = Service();
    int font = ResolveFont(px, bold);
    if (!service || font == kInvalidTextFont)
        return 0.0f;

    float pen = 0.0f;
    for (const char* cursor = utf8; *cursor;)
    {
        if (const TextGlyph* glyph = service->GetGlyph(font, NextCodepoint(cursor)))
            pen += glyph->advance;
    }
    return pen;
}

std::string Cs2Text::Fit(const char* utf8, int px, bool bold, float max_w)
{
    if (Measure(utf8, px, bold) <= max_w)
        return utf8;

    std::string out;
    for (const char* cursor = utf8; *cursor;)
    {
        const char* start = cursor;
        NextCodepoint(cursor);
        std::string next = out + std::string(start, cursor);
        if (Measure((next + "...").c_str(), px, bold) > max_w)
            break;
        out = next;
    }
    return out + "...";
}

float Cs2Text::Draw(const char* utf8, ncl_math::Vector2 pos, int px, bool bold, ncl_math::Color color, Align align)
{
    TextRenderInterface* service = Service();
    int font = ResolveFont(px, bold);
    if (!service || font == kInvalidTextFont)
        return 0.0f;

    float width = Measure(utf8, px, bold);
    float x = pos.x;
    if (align == Align::Center)
        x -= width * 0.5f;
    else if (align == Align::Right)
        x -= width;

    // whole pixels: the cells are rasterized hinted at this size
    float pen = std::round(x);
    float top = std::round(pos.y);

    for (const char* cursor = utf8; *cursor;)
    {
        const TextGlyph* glyph = service->GetGlyph(font, NextCodepoint(cursor));
        if (!glyph)
            continue;

        if (glyph->renderable)
        {
            float gx = pen + glyph->bearing_x;
            float gy = top + glyph->bearing_y;
            ncl_math::Aabb2 rect = {{gx, gy}, {gx + glyph->wide, gy + glyph->tall}};
            ncl_math::Aabb2 uv = {{glyph->u0, glyph->v0}, {glyph->u1, glyph->v1}};
            draw_.DrawGlyph(glyph->texture, rect, uv, color);
        }
        pen += glyph->advance;
    }
    return width;
}
