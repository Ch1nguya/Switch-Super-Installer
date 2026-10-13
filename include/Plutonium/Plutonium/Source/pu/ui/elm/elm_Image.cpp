#include <pu/ui/elm/elm_Image.hpp>

namespace pu::ui::elm
{
    Image::Image(s32 X, s32 Y, const std::string& Image)
        : Element::Element(), x(X), y(Y)
    {
        render::DeleteTexture(this->ntex);
        this->rendopts = render::NativeTextureRenderOptions::Default;
        this->SetImage(Image);
    }

    Image::Image(s32 X, s32 Y, void* jpegbuffer, s32 size)
        : Element::Element(), x(X), y(Y)
    {
        render::DeleteTexture(this->ntex);
        this->rendopts = render::NativeTextureRenderOptions::Default;
        this->SetJpegImage(jpegbuffer, size);
    }

    Image::Image(s32 X, s32 Y, void* rgbBuffer, u64 width, u64 height, u8 depth)
        : Element::Element(), x(X), y(Y)
    {
        render::DeleteTexture(this->ntex);
        this->rendopts = render::NativeTextureRenderOptions::Default;
        this->SetRgbImage(rgbBuffer, width, height, depth);
    }

    Image::~Image()
    {
        if(this->ntex != nullptr)
            render::DeleteTexture(this->ntex);
    }

    s32 Image::GetX()
    {
        return this->x;
    }

    void Image::SetX(s32 X)
    {
        this->x = X;
    }

    s32 Image::GetY()
    {
        return this->y;
    }

    void Image::SetY(s32 Y)
    {
        this->y = Y;
    }

    s32 Image::GetWidth()
    {
        return this->rendopts.Width;
    }

    void Image::SetWidth(s32 Width)
    {
        this->rendopts.Width = Width;
    }

    s32 Image::GetHeight()
    {
        return this->rendopts.Height;
    }

    void Image::SetHeight(s32 Height)
    {
        this->rendopts.Height = Height;
    }

    float Image::GetRotation()
    {
        return this->rendopts.Angle;
    }

    void Image::SetRotation(float Angle)
    {
        this->rendopts.Angle = Angle;
    }

    void Image::SetImage(const std::string& Image)
    {
        render::DeleteTexture(this->ntex);
        std::ifstream ifs(Image);
        bool ok = ifs.good();
        ifs.close();
        if(ok)    
        {
            this->ntex = render::LoadImage(Image);
            auto [w,h] = render::GetTextureSize(this->ntex);
            this->rendopts.Width = w;
            this->rendopts.Height = h;
        }
    }

    void Image::SetJpegImage(void* buffer, s32 size)
    {
        if (size == 0)
            return;
        render::DeleteTexture(this->ntex);
        this->ntex = render::LoadJpegImage(buffer, size);
        auto [w,h] = render::GetTextureSize(this->ntex);
        this->rendopts.Width = w;
        this->rendopts.Height = h;
    }

    void Image::SetRgbImage(void* buffer, u64 width, u64 height, u8 depth)
    {
        render::DeleteTexture(this->ntex);
        this->ntex = render::LoadRgbImage(buffer, width, height, depth);
        auto [w,h] = render::GetTextureSize(this->ntex);
        this->rendopts.Width = w;
        this->rendopts.Height = h;
    }

    bool Image::IsImageValid()
    {
        return ntex != NULL;
    }

    void Image::OnRender(render::Renderer::Ref &Drawer, s32 X, s32 Y)
    {
        Drawer->RenderTexture(this->ntex, X, Y, this->rendopts);
    }

    void Image::OnInput(u64 Down, u64 Up, u64 Held, Touch Pos)
    {
    }
}