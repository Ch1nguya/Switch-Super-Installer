
/*

    Plutonium library

    @file render_SDL2.hpp
    @brief Wrapper code to simplify SDL2 usage
    @author XorTroll

    @copyright Plutonium project - an easy-to-use UI framework for Nintendo Switch homebrew

*/

#pragma once
#include <string>
#include <vector>
#include <pu/ui/ui_Types.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <pu/ui/render/SDL_ttf.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_mixer.h>

namespace pu::ui::render
{
    typedef SDL_Window *NativeWindow;
    typedef SDL_Renderer *NativeRenderer;
    typedef SDL_Surface *NativeSurface;
    typedef SDL_Texture *NativeTexture;
    typedef TTF_Font *NativeFont;

    enum class SharedFont
    {
        Standard,
        ChineseSimplified,
        ExtendedChineseSimplified,
        ChineseTraditional,
        Korean,
        NintendoExtended,
    };

    NativeTexture ConvertToTexture(NativeSurface Surface);
    NativeTexture RenderText(NativeFont Font, NativeFont Meme, const std::string& Text, Color Color);
    NativeTexture LoadImage(const std::string& Path);
    NativeTexture LoadJpegImage(void* buffer, s32 size);
    NativeTexture LoadRgbImage(void* buffer, u64 width, u64 height, u8 depth);
    NativeFont LoadSharedFont(SharedFont Type, s32 Size);
    NativeFont LoadFont(const std::string& Path, s32 Size);
    void SetDefaultFont(const std::string& Path);
    void SetDefaultFontFromShared(SharedFont Type);
    NativeFont LoadDefaultFont(s32 Size);
    s32 GetTextureWidth(NativeTexture Texture);
    s32 GetTextureHeight(NativeTexture Texture);
    std::pair<s32,s32> GetTextureSize(NativeTexture Texture);
    void SetAlphaValue(NativeTexture Texture, u8 Alpha);
    void DeleteFont(NativeFont& Font);
    void DeleteTexture(NativeTexture& Texture);
}