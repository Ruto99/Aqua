#include "AquaTheme.h"

#include <Ruto/EditorUI/EditorUI.h>

#include <IconsFontAwesome6.h>

#include <cmath>

namespace AquaTool {

namespace {

ImVec4 Alpha(ImVec4 c, float a) {
    return {c.x, c.y, c.z, a};
}

} // namespace

void ApplyAquaStyle(float scale) {
    ImGuiStyle& s = ImGui::GetStyle();
    const float fontSize = s.FontSizeBase; // EditorUI が決めた大きさ（拡大率込み）はそのまま
    s = ImGuiStyle();
    // 余白を広めに、角を丸く（パネルは隙間を空けて浮かせる）
    s.WindowPadding = {12.0f, 10.0f};
    s.FramePadding = {10.0f, 5.0f};
    s.CellPadding = {6.0f, 4.0f};
    s.ItemSpacing = {8.0f, 6.0f};
    s.ItemInnerSpacing = {6.0f, 4.0f};
    s.IndentSpacing = 18.0f;
    s.ScrollbarSize = 10.0f;
    s.GrabMinSize = 12.0f;
    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize = 1.0f;
    s.PopupBorderSize = 1.0f;
    s.FrameBorderSize = 1.0f;
    s.TabBorderSize = 0.0f;
    s.TabBarBorderSize = 0.0f;
    s.TabBarOverlineSize = 0.0f;
    s.WindowRounding = 10.0f;
    s.ChildRounding = 8.0f;
    s.FrameRounding = 8.0f;
    s.PopupRounding = 10.0f;
    s.ScrollbarRounding = 10.0f;
    s.GrabRounding = 8.0f;
    s.TabRounding = 8.0f;
    s.WindowTitleAlign = {0.0f, 0.5f};
    s.WindowMenuButtonPosition = ImGuiDir_None;
    s.SeparatorTextBorderSize = 2.0f;
    s.SeparatorTextPadding = {0.0f, 4.0f};
    s.DockingSeparatorSize = 6.0f;

    const ImVec4 bg0 = Theme::kBackground;
    const ImVec4 bg1 = Theme::kPanel;
    const ImVec4 frame = Theme::kFrame;
    const ImVec4 hover = Theme::kFrameHover;
    const ImVec4 active = Theme::kFrameActive;
    const ImVec4 accent = Theme::kAccent;
    const ImVec4 text = Theme::kText;
    const ImVec4 dim = Theme::kTextDim;
    const ImVec4 border = Theme::kBorder;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = text;
    c[ImGuiCol_TextDisabled] = dim;
    c[ImGuiCol_WindowBg] = bg1;
    c[ImGuiCol_ChildBg] = {0, 0, 0, 0};
    c[ImGuiCol_PopupBg] = {0.047f, 0.090f, 0.114f, 0.98f};
    c[ImGuiCol_Border] = border;
    c[ImGuiCol_BorderShadow] = {0, 0, 0, 0};
    c[ImGuiCol_FrameBg] = frame;
    c[ImGuiCol_FrameBgHovered] = hover;
    c[ImGuiCol_FrameBgActive] = active;
    c[ImGuiCol_TitleBg] = bg0;
    c[ImGuiCol_TitleBgActive] = bg0;
    c[ImGuiCol_TitleBgCollapsed] = bg0;
    c[ImGuiCol_MenuBarBg] = bg0;
    c[ImGuiCol_ScrollbarBg] = {0, 0, 0, 0};
    c[ImGuiCol_ScrollbarGrab] = {0.137f, 0.247f, 0.290f, 1.0f};
    c[ImGuiCol_ScrollbarGrabHovered] = {0.176f, 0.318f, 0.369f, 1.0f};
    c[ImGuiCol_ScrollbarGrabActive] = accent;
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = Alpha(accent, 0.85f);
    c[ImGuiCol_SliderGrabActive] = accent;
    c[ImGuiCol_Button] = frame;
    c[ImGuiCol_ButtonHovered] = hover;
    c[ImGuiCol_ButtonActive] = active;
    c[ImGuiCol_Header] = Alpha(accent, 0.18f);
    c[ImGuiCol_HeaderHovered] = Alpha(accent, 0.26f);
    c[ImGuiCol_HeaderActive] = Alpha(accent, 0.38f);
    c[ImGuiCol_Separator] = border;
    c[ImGuiCol_SeparatorHovered] = Alpha(accent, 0.7f);
    c[ImGuiCol_SeparatorActive] = accent;
    c[ImGuiCol_ResizeGrip] = {0, 0, 0, 0};
    c[ImGuiCol_ResizeGripHovered] = Alpha(accent, 0.6f);
    c[ImGuiCol_ResizeGripActive] = accent;
    c[ImGuiCol_InputTextCursor] = accent;
    // タブ: 奥の帯の上に、選んだタブだけアクアがかった札として浮かぶ
    c[ImGuiCol_Tab] = bg0;
    c[ImGuiCol_TabHovered] = hover;
    c[ImGuiCol_TabSelected] = {0.071f, 0.204f, 0.224f, 1.0f};
    c[ImGuiCol_TabSelectedOverline] = {0, 0, 0, 0};
    c[ImGuiCol_TabDimmed] = bg0;
    c[ImGuiCol_TabDimmedSelected] = bg1;
    c[ImGuiCol_TabDimmedSelectedOverline] = {0, 0, 0, 0};
    c[ImGuiCol_DockingPreview] = Alpha(accent, 0.45f);
    c[ImGuiCol_DockingEmptyBg] = bg0;
    c[ImGuiCol_PlotLines] = accent;
    c[ImGuiCol_PlotHistogram] = accent;
    c[ImGuiCol_TableHeaderBg] = bg0;
    c[ImGuiCol_TableBorderStrong] = border;
    c[ImGuiCol_TableBorderLight] = Alpha(border, 0.6f);
    c[ImGuiCol_TableRowBg] = {0, 0, 0, 0};
    c[ImGuiCol_TableRowBgAlt] = {0.6f, 1.0f, 1.0f, 0.025f};
    c[ImGuiCol_TextSelectedBg] = Alpha(accent, 0.30f);
    c[ImGuiCol_DragDropTarget] = Theme::kCoral;
    c[ImGuiCol_NavCursor] = accent;
    c[ImGuiCol_ModalWindowDimBg] = {0.0f, 0.03f, 0.05f, 0.65f};

    s.ScaleAllSizes(scale);
    s.FontSizeBase = fontSize;
}

bool IconButton(const char* icon, const char* tooltip, bool selected, ImVec2 size) {
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, Theme::kAccent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.40f, 0.92f, 0.88f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::kAccent);
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::kAccentInk);
        ImGui::PushStyleColor(ImGuiCol_Border, {0, 0, 0, 0});
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
        ImGui::PushStyleColor(ImGuiCol_Border, {0, 0, 0, 0});
    }
    if (size.x <= 0.0f) size = {ImGui::GetFrameHeight() * 1.2f, ImGui::GetFrameHeight()};
    const bool pressed = ImGui::Button(icon, size);
    ImGui::PopStyleColor(selected ? 5 : 2);
    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", tooltip);
    return pressed;
}

void DrawLogo(float size) {
    // しずくをアクアの丸に白抜きで置き、横に太字の「Aqua」
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float d = std::round(size * 1.35f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 center{pos.x + d * 0.5f, pos.y + d * 0.5f};
    dl->AddCircleFilled(center, d * 0.5f, Theme::U32(Theme::kAccent));
    ImGui::PushFont(nullptr, size * 0.8f);
    const ImVec2 is = ImGui::CalcTextSize(ICON_FA_DROPLET);
    dl->AddText({center.x - is.x * 0.5f, center.y - is.y * 0.5f}, Theme::U32(Theme::kAccentInk), ICON_FA_DROPLET);
    ImGui::PopFont();
    ImGui::Dummy({d, d});
    ImGui::SameLine(0.0f, size * 0.45f);
    ImGui::PushFont(Ruto::EditorUI::GetBoldFont(), size);
    ImGui::SetCursorScreenPos({ImGui::GetCursorScreenPos().x, pos.y + (d - ImGui::GetTextLineHeight()) * 0.5f});
    ImGui::TextColored(Theme::kText, "Aqua");
    ImGui::PopFont();
}

} // namespace AquaTool
