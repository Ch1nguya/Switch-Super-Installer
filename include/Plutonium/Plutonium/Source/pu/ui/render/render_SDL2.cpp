#include <pu/ui/render/render_SDL2.hpp>
#include <pu/ui/render/render_Renderer.hpp>
#include <utility>
#include <unordered_map>

namespace pu::ui::render
{
    std::unordered_map<u32, std::unordered_map<std::string, NativeFont>> filefonts;
    std::unordered_map<u32, std::unordered_map<SharedFont, NativeFont>> shfonts;

    static SharedFont shfont = SharedFont::Standard;
    static std::string fontpth;

    NativeTexture ConvertToTexture(NativeSurface Surface)
    {
        NativeTexture tex = SDL_CreateTextureFromSurface(GetMainRenderer(), Surface);
        SDL_FreeSurface(Surface);
        return tex;
    }

    NativeTexture RenderText(NativeFont Font, NativeFont Meme, const std::string& Text, Color Color)
    {
        NativeSurface txsrf = TTF_RenderUTF8_Blended_Wrapped(Font, Meme, Text.c_str(), { Color.R, Color.G, Color.B, Color.A }, 1280);
        SDL_SetSurfaceAlphaMod(txsrf, 255);
        return ConvertToTexture(txsrf);
    }

    NativeTexture LoadImage(const std::string& Path)
    {
        return ConvertToTexture(IMG_Load(Path.c_str()));
    }

    NativeTexture LoadJpegImage(void* buffer, s32 size)
    {
        return ConvertToTexture(IMG_Load_RW(SDL_RWFromMem(buffer, size), size));
    }

    NativeTexture LoadRgbImage(void* buffer, u64 width, u64 height, u8 depth) {
        return ConvertToTexture(SDL_CreateRGBSurfaceFrom(buffer, width, height, depth*8, depth*width, 0x000000ff, 0x0000ff00, 0x00ff0000, depth == 4 ? 0xff000000 : 0));
    }

    NativeFont LoadSharedFont(SharedFont Type, s32 Size)
    {
        auto it_size = shfonts.find(Size);
        if(it_size != shfonts.end()) {
            auto it_path = it_size->second.find(Type);
            if (it_path != it_size->second.end()) {
                return it_path->second;
            }
        }
        PlFontData plfont;
        NativeFont font = NULL;
        SDL_RWops *mem = NULL;
        Result rc = plGetSharedFontByType(&plfont, static_cast<PlSharedFontType>(Type));
        if(rc == 0)
        {
            mem = SDL_RWFromMem(plfont.address, plfont.size);
            font = TTF_OpenFontRW(mem, 1, Size);
        }
        if(font != NULL) {
            if(it_size == shfonts.end()) {
                std::unordered_map<SharedFont, NativeFont> typeMap;
                typeMap.insert(std::make_pair(Type, font));
                shfonts.insert(std::make_pair(Size, typeMap));
            } else {
                it_size->second.insert(std::make_pair(Type, font));
            }
        }
        return font;
    }

    NativeFont LoadFont(const std::string& Path, s32 Size)
    {
        auto it_size = filefonts.find(Size);
        if(it_size != filefonts.end()) {
            auto it_path = it_size->second.find(Path);
            if (it_path != it_size->second.end()) {
                return it_path->second;
            }
        }
        auto font = TTF_OpenFont(Path.c_str(), Size);
        if(font != NULL) {
            if(it_size == filefonts.end()) {
                std::unordered_map<std::string, NativeFont> pathMap;
                pathMap.insert(std::make_pair(Path, font));
                filefonts.insert(std::make_pair(Size, pathMap));
            } else {
                it_size->second.insert(std::make_pair(Path, font));
            }
        }
        return font;
    }

    void SetDefaultFont(const std::string& Path)
    {
        fontpth = Path;
    }

    void SetDefaultFontFromShared(SharedFont Type)
    {
        shfont = Type;
    }

    NativeFont LoadDefaultFont(s32 Size)
    {
        if(!fontpth.empty()) return LoadFont(fontpth, Size);
        return LoadSharedFont(shfont, Size);
    }

    s32 GetTextureWidth(NativeTexture Texture)
    {
        int w = 0;
        SDL_QueryTexture(Texture, NULL, NULL, &w, NULL);
        return (s32)w;
    }

    s32 GetTextureHeight(NativeTexture Texture)
    {
        int h = 0;
        SDL_QueryTexture(Texture, NULL, NULL, NULL, &h);
        return (s32)h;
    }

    std::pair<s32,s32> GetTextureSize(NativeTexture Texture)
    {
        int w, h;
        SDL_QueryTexture(Texture, NULL, NULL, &w, &h);
        return std::make_pair(w, h);
    }

    void SetAlphaValue(NativeTexture Texture, u8 Alpha)
    {
        SDL_SetTextureBlendMode(Texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(Texture, Alpha);
    }

    void DeleteFont(NativeFont& Font)
    {
        if (Font != nullptr) {
            TTF_CloseFont(Font);
            Font = NULL;
        }
    }

    void DeleteTexture(NativeTexture& Texture)
    {
        if (Texture != nullptr) {
            SDL_DestroyTexture(Texture);
            Texture = NULL;
        }
    }
}