#include <pu/ui/elm/elm_TextBlock.hpp>

namespace pu::ui::elm
{
    TextBlock::TextBlock(s32 X, s32 Y, const std::string& Text, s32 FontSize)
        : Element::Element(), x(X), y(Y), text(Text)
    {
        this->clr = { 0, 0, 0, 255 };
        this->SetFontSize(FontSize);
    }

    TextBlock::~TextBlock()
    {
        render::DeleteTexture(this->ntex);
    }

    s32 TextBlock::GetX()
    {
        return this->x;
    }

    void TextBlock::SetX(s32 X)
    {
        this->x = X;
    }

    s32 TextBlock::GetY()
    {
        return this->y;
    }

    void TextBlock::SetY(s32 Y)
    {
        this->y = Y;
    }

    s32 TextBlock::GetWidth()
    {
        return this->GetTextWidth();
    }

    s32 TextBlock::GetHeight()
    {
        return this->GetTextHeight();
    }

    s32 TextBlock::GetTextWidth()
    {
        return render::GetTextureWidth(this->ntex);
    }

    s32 TextBlock::GetTextHeight()
    {
        return render::GetTextureHeight(this->ntex);
    }

    std::string TextBlock::GetText()
    {
        return this->text;
    }

    void TextBlock::SetText(const std::string& Text)
    {
        this->text = Text;
        render::DeleteTexture(this->ntex);
        this->ntex = render::RenderText(this->font, this->meme, Text, this->clr);
    }

    void TextBlock::SetFontSize(s32 FontSize)
    {
        this->font = render::LoadDefaultFont(FontSize);
        this->meme = render::LoadSharedFont(render::SharedFont::NintendoExtended, FontSize);
        render::DeleteTexture(this->ntex);
        this->ntex = render::RenderText(this->font, this->meme, this->text, this->clr);
    }

    Color TextBlock::GetColor()
    {
        return this->clr;
    }

    void TextBlock::SetColor(Color Color)
    {
        this->clr = Color;
        render::DeleteTexture(this->ntex);
        this->ntex = render::RenderText(this->font, this->meme, this->text, Color);
    }

    void TextBlock::OnRender(render::Renderer::Ref &Drawer, s32 X, s32 Y)
    {
        s32 rdx = X;
        s32 rdy = Y;
        Drawer->RenderTexture(this->ntex, rdx, rdy);
    }

    void TextBlock::OnInput(u64 Down, u64 Up, u64 Held, Touch Pos)
    {
    }
}