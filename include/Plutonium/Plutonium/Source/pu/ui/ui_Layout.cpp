#include <pu/ui/ui_Layout.hpp>

namespace pu::ui
{
    Layout::Layout() : Container(0, 0, 1280, 720)
    {
        this->onipt = [&](u64,u64,u64,Touch){};
        this->hasimage = false;
        this->overbgtex = NULL;
        this->overbgcolor = Color(225, 225, 225, 255);
    }

    Layout::~Layout()
    {
        render::DeleteTexture(this->overbgtex);
    }

    bool Layout::HasChilds()
    {
        return !this->elms.empty();
    }

    void Layout::SetOnInput(std::function<void(u64 Down, u64 Up, u64 Held, Touch Pos)> Callback)
    {
        this->onipt = Callback;
    }

    std::function<void(u64 Down, u64 Up, u64 Held, Touch Pos)> Layout::GetOnInput()
    {
        return this->onipt;
    }

    void Layout::AddThread(std::function<void()> Callback)
    {
        this->thds.push_back(Callback);
    }

    std::vector<std::function<void()>> Layout::GetAllThreads()
    {
        return this->thds;
    }

    void Layout::SetBackgroundImage(const std::string& Path)
    {
        render::DeleteTexture(this->overbgtex);
        this->hasimage = true;
        this->overbgtex = render::LoadImage(Path);
    }

    void Layout::SetBackgroundJpegImage(void* JpegBuffer, s32 size)
    {
        render::DeleteTexture(this->overbgtex);
        this->hasimage = true;
        this->overbgtex = render::LoadJpegImage(JpegBuffer, size);
    }

    void Layout::SetBackgroundRgbImage(void* RgbBuffer, u64 width, u64 height, u8 depth) {
        render::DeleteTexture(this->overbgtex);
        this->hasimage = true;
        this->overbgtex = render::LoadRgbImage(RgbBuffer, width, height, depth);
    }

    void Layout::SetBackgroundColor(Color Color)
    {
        render::DeleteTexture(this->overbgtex);
        this->hasimage = false;
        this->overbgcolor = Color;
    }

    void Layout::SimulateTouch(Touch Custom)
    {
        this->simtouch = Custom;
    }

    Touch Layout::GetSimulatedTouch()
    {
        Touch simcpy = this->simtouch;
        this->simtouch = Touch::Empty;
        return simcpy; // Getting simulated touch resets it
    }

    render::NativeTexture Layout::GetBackgroundImageTexture()
    {
        return this->overbgtex;
    }

    Color Layout::GetBackgroundColor()
    {
        return this->overbgcolor;
    }

    bool Layout::HasBackgroundImage()
    {
        return this->hasimage;
    }
}