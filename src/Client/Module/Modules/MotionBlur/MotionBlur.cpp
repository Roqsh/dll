#include "MotionBlur.hpp"
#include "Client.hpp"

MotionBlur::MotionBlur(): Module("Motion Blur",
                                 "Make fast movements appear smoother and more realistic by\nblurring the image slightly in the direction of motion.",
                                 IDR_BLUR_PNG, "")
{
    //this->setup();

}

void MotionBlur::onEnable()
{
    if (SwapchainHook::queue) { if (!once) { FlarialGUI::Notify("Please turn on Better Frames in Settings!"); once = true; } }
    else {
        ListenOrdered(this, RenderUnderUIEvent, &MotionBlur::onRenderUnderUI, EventOrder::IMMEDIATE)
        ListenOrdered(this, RenderEvent, &MotionBlur::onRender, EventOrder::IMMEDIATE)
        Module::onEnable();
    }
}

void MotionBlur::onDisable()
{
    Deafen(this, RenderUnderUIEvent, &MotionBlur::onRenderUnderUI)
        Deafen(this, RenderEvent, &MotionBlur::onRender, EventOrder::IMMEDIATE)
    previousFrames.clear();
    Module::onDisable();
}

void MotionBlur::defaultConfig()
{
    Module::defaultConfig("core");
    setDef("intensity", 0.88f);
    setDef("intensity2", 6.0f);
    setDef("avgpixel", false);
    setDef("dynamic", true);
    setDef("samples", 64.f);
    setDef("RenderOverUI", false);
    if (ModuleManager::initialized) Client::SaveSettings();
}

void MotionBlur::settingsRender(float settingsOffset)
{
    float x = Constraints::PercentageConstraint(0.019, "left");
    float y = Constraints::PercentageConstraint(0.10, "top");

    const float scrollviewWidth = Constraints::RelativeConstraint(0.12, "height", true);


    FlarialGUI::ScrollBar(x, y, 140, Constraints::SpacingConstraint(5.5, scrollviewWidth), 2);
    FlarialGUI::SetScrollView(x - settingsOffset, Constraints::PercentageConstraint(0.00, "top"),
                              Constraints::RelativeConstraint(1.0, "width"),
                              Constraints::RelativeConstraint(0.88f, "height"));

    addHeader("Motion Blur");
    addToggle("Average Pixel Mode", "Disabling this will likely look better on high FPS.", "avgpixel");

    addConditionalToggle(getOps<bool>("avgpixel"), "Dynamic Mode", "Automatically adjusts intensity according to FPS", "dynamic");
    addConditionalSlider(getOps<bool>("avgpixel") && !getOps<bool>("dynamic"), "Intensity", "Amount of previous frames to render.", "intensity2", 30, 0, true);
    addConditionalToggle(getOps<bool>("avgpixel"), "HUD Motion Blur", "Applies motion blur to HUD elements like hand.", "RenderOverUI");

    addConditionalSlider(!getOps<bool>("avgpixel"), "Intensity", "Control how strong the motion blur is.", "intensity", 2, 0.05f, true);
    addConditionalSlider(!getOps<bool>("avgpixel"), "Intensity", "Control how strong the motion blur is.", "intensity", 2, 0.05f, true);

    FlarialGUI::UnsetScrollView();

    resetPadding();
}

void MotionBlur::onRenderUnderUI(RenderUnderUIEvent& event)
{
    if (!this->isEnabled()) return;
    if (SwapchainHook::queue) return;
    if (getOps<bool>("avgpixel") and getOps<bool>("RenderOverUI")) return;

    int maxFrames = (int)round(getOps<float>("intensity2"));

    if (getOps<bool>("dynamic")) {
        if (MC::fps < 75) maxFrames = 1;
        else if (MC::fps < 100) maxFrames = 2;
        else if (MC::fps < 180) maxFrames = 3;
        else if (MC::fps > 300) maxFrames = 4;
        else if (MC::fps > 450) maxFrames = 5;
    }

    if (getOps<bool>("avgpixel")) maxFrames = 1;

    if (SDK::getCurrentScreen() == "hud_screen" && initted && this->isEnabled()) {

        // Remove excess frames if maxFrames is reduced
        if (previousFrames.size() > static_cast<size_t>(maxFrames)) {
            previousFrames.erase(previousFrames.begin(),
                                 previousFrames.begin() + (previousFrames.size() - maxFrames));
        }

        auto buffer = BackbufferToSRVExtraMode();
        if (buffer) {
            previousFrames.push_back(std::move(buffer));
        }

        if (getOps<bool>("avgpixel"))
            if (!getOps<bool>("RenderOverUI"))
                AvgPixelMotionBlurHelper::Render(event.RTV, previousFrames);
        else RealMotionBlurHelper::Render(event.RTV, previousFrames.back());

    }
    else {
        previousFrames.clear();
    }
}

void MotionBlur::onRender(RenderEvent& event) {
    if (!this->isEnabled()) return;
    if (SwapchainHook::queue) return;

    if (getOps<bool>("avgpixel")) {
        if (!getOps<bool>("RenderOverUI")) {
            return;
        }
    }
    else {
        return;
    }


    int maxFrames = (int)round(getOps<float>("intensity2"));

    if (getOps<bool>("dynamic")) {
        if (MC::fps < 75) maxFrames = 1;
        else if (MC::fps < 100) maxFrames = 2;
        else if (MC::fps < 180) maxFrames = 3;
        else if (MC::fps > 300) maxFrames = 4;
        else if (MC::fps > 450) maxFrames = 5;
    }

    if (getOps<bool>("avgpixel")) maxFrames = 1;

    if (SDK::getCurrentScreen() == "hud_screen" && initted && this->isEnabled()) {

        // Remove excess frames if maxFrames is reduced
        if (previousFrames.size() > static_cast<size_t>(maxFrames)) {
            previousFrames.erase(previousFrames.begin(),
                previousFrames.begin() + (previousFrames.size() - maxFrames));
        }

        auto buffer = BackbufferToSRVExtraMode();
        if (buffer) {
            previousFrames.push_back(std::move(buffer));
        }

        AvgPixelMotionBlurHelper::Render(event.RTV, previousFrames);

    }
    else {
        previousFrames.clear();
    }
}

void MotionBlur::ImageWithOpacity(const winrt::com_ptr<ID3D11ShaderResourceView>& srv, ImVec2 size, float opacity)
{
    if (opacity <= 0.0f) {
        //std::cout << "alpha: " + FlarialGUI::cached_to_string(opacity) << std::endl;
        return;
    }

    opacity = opacity > 1.0f ? 1.0f : opacity < 0.0f ? 0.0f : opacity;
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    ImVec2 pos = { 0, 0 };
    ImU32 col = IM_COL32(255, 255, 255, static_cast<int>(opacity * 255));
    draw_list->AddImage(ImTextureID(srv.get()), pos, ImVec2(pos.x + size.x, pos.y + size.y), ImVec2(0, 0), ImVec2(1, 1), col);
    ImGui::SetCursorScreenPos(ImVec2(pos.x + size.x, pos.y));
}

winrt::com_ptr<ID3D11ShaderResourceView> MotionBlur::BackbufferToSRVExtraMode()
{

    if (!FlarialGUI::needsBackBuffer) return nullptr;
    if (SwapchainHook::queue) return BackbufferToSRV();
    HRESULT hr;

    D3D11_TEXTURE2D_DESC d;
    SwapchainHook::ExtraSavedD3D11BackBuffer->GetDesc(&d);
    winrt::com_ptr<ID3D11ShaderResourceView> outSRV;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = d.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = d.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    if (FAILED(hr = SwapchainHook::d3d11Device->CreateShaderResourceView(SwapchainHook::ExtraSavedD3D11BackBuffer, &srvDesc, outSRV.put())))
    {
        std::cout << "Failed to create shader resource view: " << std::hex << hr << std::endl;
    }

    return outSRV;
}

winrt::com_ptr<ID3D11ShaderResourceView> MotionBlur::BackbufferToSRV()
{

    HRESULT hr;

    D3D11_TEXTURE2D_DESC d;
    SwapchainHook::SavedD3D11BackBuffer->GetDesc(&d);
    winrt::com_ptr<ID3D11ShaderResourceView> outSRV;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = d.Format;

    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = d.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    if (FAILED(hr = SwapchainHook::d3d11Device->CreateShaderResourceView(SwapchainHook::SavedD3D11BackBuffer, &srvDesc, outSRV.put())))
    {
        std::cout << "Failed to create shader resource view: " << std::hex << hr << std::endl;
    }

    return outSRV;
}
