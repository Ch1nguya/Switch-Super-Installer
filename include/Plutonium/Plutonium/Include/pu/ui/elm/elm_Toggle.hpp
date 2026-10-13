
/*

    Plutonium library

    @file Toggle.hpp
    @brief A Toggle is an Element used to switch between two options by toggling the item.
    @author XorTroll

    @copyright Plutonium project - an easy-to-use UI framework for Nintendo Switch homebrew

*/

#pragma once
#include <pu/ui/elm/elm_Element.hpp>

namespace pu::ui::elm
{
    constexpr u64 TouchPseudoKey = HidNpadButton_Verification;

    class Toggle : public Element
    {
        public:
            Toggle(s32 X, s32 Y, const std::string& Content, u64 Key, Color Color);
            PU_SMART_CTOR(Toggle)
            ~Toggle();

            s32 GetX();
            void SetX(s32 X);
            s32 GetY();
            void SetY(s32 Y);
            s32 GetWidth();
            s32 GetHeight();
            std::string GetContent();
            void SetContent(const std::string& Content);
            void SetFontSize(s32 FontSize);
            Color GetColor();
            void SetColor(Color General);
            u64 GetKey();
            void SetKey(u64 Key);
            bool IsChecked();
            void OnRender(render::Renderer::Ref &Drawer, s32 X, s32 Y);
            void OnInput(u64 Down, u64 Up, u64 Held, Touch Pos);
        private:
            std::string cnt;
            s32 x;
            s32 y;
            u64 key;
            bool checked;
            Color clr;
            render::NativeFont font;
            render::NativeFont meme;
            s32 togfact;
            render::NativeTexture ntex = nullptr;
    };
}