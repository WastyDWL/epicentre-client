#include "Cs2Avatars.h"

#include <vector>

#include <glad/glad.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <steam/isteamfriends.h>
#include <steam/isteamutils.h>

namespace
{
    // client.dll's steam_api.dll (NextClient's proxy) is already loaded; nothing is linked here
    struct SteamInterfaces
    {
        ISteamFriends* friends = nullptr;
        ISteamUtils* utils = nullptr;
    };

    SteamInterfaces Steam()
    {
        SteamInterfaces out;
#ifdef _WIN32
        if (HMODULE module = GetModuleHandleW(L"steam_api.dll"))
        {
            using FriendsFn = ISteamFriends* (*)();
            using UtilsFn = ISteamUtils* (*)();
            auto friends = reinterpret_cast<FriendsFn>(GetProcAddress(module, "SteamFriends"));
            auto utils = reinterpret_cast<UtilsFn>(GetProcAddress(module, "SteamUtils"));
            out.friends = friends ? friends() : nullptr;
            out.utils = utils ? utils() : nullptr;
        }
#endif
        return out;
    }

    constexpr float kRetry = 0.5f; // seconds between Steam queries for one player
} // namespace

unsigned int Cs2Avatars::Texture(uint64_t steam_id, float now)
{
    if (steam_id == 0)
        return 0;

    Entry& e = entries_[steam_id];
    if (e.texture || now < e.next_try)
        return e.texture;
    e.next_try = now + kRetry;

    SteamInterfaces steam = Steam();
    if (!steam.friends || !steam.utils)
        return 0;

    CSteamID id(static_cast<uint64>(steam_id));
    int image = steam.friends->GetMediumFriendAvatar(id);
    if (image == 0)
    {
        // no avatar cached for this user: ask Steam once, the image follows a little later
        if (!e.requested)
        {
            steam.friends->RequestUserInformation(id, false);
            e.requested = true;
        }
        return 0;
    }
    if (image < 0)
        return 0; // still loading

    uint32 w = 0, h = 0;
    if (!steam.utils->GetImageSize(image, &w, &h) || w == 0 || h == 0 || w > 256 || h > 256)
        return 0;
    std::vector<uint8> rgba(static_cast<size_t>(w) * h * 4);
    if (!steam.utils->GetImageRGBA(image, rgba.data(), static_cast<int>(rgba.size())))
        return 0;

    GLint prev = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev);
    glGenTextures(1, &e.texture);
    glBindTexture(GL_TEXTURE_2D, e.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev));
    return e.texture;
}
