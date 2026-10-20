// アニメーション: 一覧・再生・録画・トラックとキー（Unreal の Widget Animation）

#include "AquaApp.h"

#include <Ruto/EditorUI/EditorUI.h>
#include <Ruto/EditorUI/Widgets.h>

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <format>

namespace AquaTool {

using namespace Ruto::EditorUI;

void App::DrawAnimations(Document& doc) {
    WidgetAsset& asset = *doc.asset;
    DesignerState& ui = doc.ui;
    const float s = m_Scale;
    const ImGuiIO& io = ImGui::GetIO();

    if (ui.animation >= static_cast<int>(asset.animations.size())) ui.animation = -1;
    WidgetAnimation* anim = ui.animation >= 0 ? &asset.animations[static_cast<size_t>(ui.animation)] : nullptr;
    // 見出し（再生・録画・時刻）
    ImGui::BeginDisabled(!anim || ui.preview);
    if (IconButton(ui.animPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY, ui.animPlaying ? "一時停止" : "再生", ui.animPlaying)) {
        ui.animPlaying = !ui.animPlaying;
        if (ui.animPlaying && anim && ui.animTime >= anim->GetDuration() - 1e-3f) ui.animTime = 0.0f;
    }
    ImGui::SameLine(0.0f, 2.0f);
    if (ui.animRecording) ImGui::PushStyleColor(ImGuiCol_Text, {0.95f, 0.3f, 0.3f, 1.0f});
    if (IconButton(ICON_FA_CIRCLE, ui.animRecording ? "録画を止める" : "録画（詳細パネルで数値を変えるとキーが打たれる）", ui.animRecording)) ui.animRecording = !ui.animRecording;
    if (ui.animRecording) ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f * s);
    ImGui::DragFloat("##anim-time", &ui.animTime, 0.01f, 0.0f, 600.0f, "%.2f 秒");
    ImGui::EndDisabled();
    if (anim) {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("/ %.2f 秒   スクリプト: PlayAnimation(AquaUI::%s::Animations::%s)", anim->GetDuration(), ToIdentifier(doc.GetTitle()).c_str(),
                            ToIdentifier(anim->name).c_str());
    }

    // 再生を進める
    if (anim && ui.animPlaying) {
        ui.animTime += io.DeltaTime;
        const float d = anim->GetDuration();
        if (d <= 1e-3f) ui.animPlaying = false;
        else if (ui.animTime > d) ui.animTime = std::fmod(ui.animTime, d);
    }

    const float listW = std::round(200.0f * s);
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("##anim-list", {listW, avail.y}, ImGuiChildFlags_Borders);
    if (ImGui::Button(ICON_FA_PLUS "  アニメーションを追加", {-FLT_MIN, 0.0f})) {
        WidgetAnimation a;
        a.name = "NewAnimation";
        for (int n = 1; asset.FindAnimation(a.name); ++n) a.name = std::format("NewAnimation{}", n);
        asset.animations.push_back(a);
        ui.animation = static_cast<int>(asset.animations.size()) - 1;
        ui.animTime = 0.0f;
        ui.animRecording = true;
        doc.Changed();
    }
    int remove = -1;
    for (int i = 0; i < static_cast<int>(asset.animations.size()); ++i) {
        WidgetAnimation& a = asset.animations[static_cast<size_t>(i)];
        ImGui::PushID(i);
        if (ui.isRenamingAnim && ui.renamingAnim == a.name) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (!ImGui::IsAnyItemActive()) ImGui::SetKeyboardFocusHere();
            if (ImGui::InputText("##ren", &ui.animRenameBuffer, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll) || ImGui::IsItemDeactivated()) {
                if (!ui.animRenameBuffer.empty() && !asset.FindAnimation(ui.animRenameBuffer)) {
                    a.name = ui.animRenameBuffer;
                    doc.Changed();
                }
                ui.isRenamingAnim = false;
            }
        } else {
            if (ImGui::Selectable((std::string(ICON_FA_WAND_MAGIC_SPARKLES "  ") + a.name).c_str(), ui.animation == i, ImGuiSelectableFlags_AllowDoubleClick)) {
                const bool changedAnim = ui.animation != i;
                ui.animation = changedAnim ? i : -1; // もう一度クリックで選択解除（アニメーションを見ない）
                ui.animTime = 0.0f;
                ui.animPlaying = false;
                if (doc.view) doc.view->SetAsset(doc.asset);
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    ui.animation = i;
                    ui.isRenamingAnim = true;
                    ui.renamingAnim = a.name;
                    ui.animRenameBuffer = a.name;
                }
            }
            if (ImGui::BeginPopupContextItem("anim-menu")) {
                if (ImGui::MenuItem(ICON_FA_PEN "  名前を変更")) {
                    ui.isRenamingAnim = true;
                    ui.renamingAnim = a.name;
                    ui.animRenameBuffer = a.name;
                }
                if (ImGui::MenuItem(ICON_FA_CLONE "  複製")) {
                    WidgetAnimation copy = a;
                    copy.name += "Copy";
                    for (int n = 2; asset.FindAnimation(copy.name); ++n) copy.name = std::format("{}Copy{}", a.name, n);
                    asset.animations.push_back(copy);
                    doc.Changed();
                }
                if (ImGui::MenuItem(ICON_FA_TRASH "  削除")) remove = i;
                ImGui::EndPopup();
            }
        }
        ImGui::PopID();
        if (remove >= 0) break;
    }
    if (remove >= 0) {
        asset.animations.erase(asset.animations.begin() + remove);
        ui.animation = -1;
        if (doc.view) doc.view->SetAsset(doc.asset);
        doc.Changed();
    }
    if (asset.animations.empty()) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("フェードイン・ボタンの登場などを作れます");
        ImGui::PopTextWrapPos();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ---- トラックとキー ----
    ImGui::BeginChild("##anim-tracks", {0.0f, avail.y}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollWithMouse);
    if (!anim) {
        ImGui::TextDisabled("左でアニメーションを選ぶと、ここにトラックとキーが表示されます");
        ImGui::TextDisabled("録画（●）中に詳細パネルの数値（不透明度・描画のずれ・色など）を変えると、キーが打たれます");
        ImGui::EndChild();
        return;
    }
    const float labelW = std::round(std::clamp(ImGui::GetContentRegionAvail().x * 0.35f, 90.0f * s, 210.0f * s));
    const float rowH = std::round(22.0f * s);
    const float rulerH = std::round(20.0f * s);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 area = ImGui::GetContentRegionAvail();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float x0 = origin.x + labelW;
    auto timeToX = [&](float t) { return x0 + 8.0f + t * ui.animZoom; };
    auto xToTime = [&](float x) { return std::max((x - x0 - 8.0f) / ui.animZoom, 0.0f); };
    auto snap = [&](float t) { return io.KeyAlt ? t : std::round(t * 60.0f) / 60.0f; };
    // 目盛り
    dl->AddRectFilled({x0, origin.y}, {origin.x + area.x, origin.y + rulerH}, ToU32(Theme::kPanel));
    float step = 0.1f;
    while (step * ui.animZoom < 50.0f * s) step *= 2.0f;
    for (float t = 0.0f; timeToX(t) < origin.x + area.x; t += step) {
        dl->AddLine({timeToX(t), origin.y + rulerH - 6.0f}, {timeToX(t), origin.y + rulerH}, ToU32(Theme::kTextDim));
        dl->AddText({timeToX(t) + 3.0f, origin.y + 2.0f}, ToU32(Theme::kTextDim), std::format("{:.1f}", t).c_str());
    }
    ImGui::SetCursorScreenPos({x0, origin.y});
    ImGui::InvisibleButton("##anim-ruler", {std::max(area.x - labelW, 1.0f), rulerH});
    if (ImGui::IsItemActive()) {
        ui.animTime = snap(xToTime(io.MousePos.x));
        ui.animPlaying = false;
    }
    if (ImGui::IsItemHovered() && io.MouseWheel != 0.0f) ui.animZoom = std::clamp(ui.animZoom * std::pow(1.15f, io.MouseWheel), 20.0f, 2000.0f);

    // トラックを追加（選択中のウィジェットのプロパティ）
    ImGui::SetCursorScreenPos({origin.x + 4.0f, origin.y});
    Widget* sel = ui.hasSelection ? Resolve(asset.root.get(), ui.selection) : nullptr;
    ImGui::BeginDisabled(!sel);
    if (ImGui::SmallButton(ICON_FA_PLUS " トラック")) ImGui::OpenPopup("add-anim-track");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("選択中のウィジェットのプロパティを追加（今の値でキーを打つ）");
    if (sel && ImGui::BeginPopup("add-anim-track")) {
        ImGui::TextDisabled("%s のプロパティ", sel->name.c_str());
        Widget* docSel = doc.view ? Resolve(doc.view->GetRoot(), ui.selection) : nullptr;
        std::string lastProp;
        for (const auto& info : WidgetProperties::List(*sel)) {
            // オン / オフの設定・回数はアニメーションしない
            static const char* const kSkip[] = {"richText", "revealAuto", "revealOnShow", "revealSoundEvery", "shaderChildren"};
            if (std::any_of(std::begin(kSkip), std::end(kSkip), [&](const char* k) { return info.property == k; })) continue;
            std::string label;
            if (ShaderParamSlot(info.property) >= 0) {
                // シェーダーの値: @param の名前で、使う成分だけ
                label = AnimPropertyLabel(*sel, info.property, info.channel);
                if (label.empty()) continue;
                if (lastProp != "shader") {
                    lastProp = "shader";
                    ImGui::TextDisabled("%s", PropertyLabelJa("shader").c_str());
                }
                label = "    " + label;
            } else {
                if (info.property != lastProp) {
                    lastProp = info.property;
                    ImGui::TextDisabled("%s", PropertyLabelJa(info.property).c_str());
                }
                label = "    " + (info.channel.empty() ? PropertyLabelJa(info.property) : info.channel);
            }
            if (ImGui::MenuItem(label.c_str(), nullptr, anim->FindTrack(sel->name, info.path) != nullptr)) {
                float v = 0.0f;
                if (!(docSel && WidgetProperties::Get(*docSel, info.path, v))) WidgetProperties::Get(*sel, info.path, v);
                WidgetAnimTrack* t = anim->FindTrack(sel->name, info.path);
                if (!t) {
                    anim->tracks.push_back({sel->name, info.path, TimelineCurve{info.path, {}}});
                    t = &anim->tracks.back();
                }
                t->curve.SetKey(ui.animTime, v);
                doc.Changed();
            }
        }
        ImGui::EndPopup();
    }

    // 行
    ImGui::SetCursorScreenPos({origin.x, origin.y + rulerH});
    ImGui::BeginChild("##anim-rows", {0.0f, 0.0f}, ImGuiChildFlags_None);
    const ImVec2 rowsOrigin = ImGui::GetCursorScreenPos();
    ImDrawList* rdl = ImGui::GetWindowDrawList();
    int removeTrack = -1;
    for (int ti = 0; ti < static_cast<int>(anim->tracks.size()); ++ti) {
        WidgetAnimTrack& t = anim->tracks[static_cast<size_t>(ti)];
        const float y = rowsOrigin.y + static_cast<float>(ti) * rowH;
        ImGui::PushID(ti);
        const bool exists = asset.root->Find(t.widget) != nullptr;
        rdl->AddRectFilled({rowsOrigin.x, y}, {rowsOrigin.x + labelW, y + rowH}, ToU32(ti % 2 ? Theme::kPanel : Theme::kFrame));
        rdl->AddLine({rowsOrigin.x, y + rowH - 0.5f}, {rowsOrigin.x + area.x, y + rowH - 0.5f}, ToU32(Theme::kBorder));
        std::string propLabel = t.curve.property;
        if (Widget* tw = asset.root->Find(t.widget)) {
            // shaderParam0.x などは @param の表示名にする
            std::string prop = t.curve.property, ch;
            if (const size_t dot = prop.rfind('.'); dot != std::string::npos && prop.size() - dot == 2) {
                ch = prop.substr(dot + 1);
                prop = prop.substr(0, dot);
            }
            if (ShaderParamSlot(prop) >= 0) {
                if (std::string l = AnimPropertyLabel(*tw, prop, ch); !l.empty()) propLabel = l;
            }
        }
        const std::string label = std::format("{}  ·  {}", t.widget, propLabel);
        rdl->PushClipRect({rowsOrigin.x, y}, {rowsOrigin.x + labelW - 4.0f, y + rowH}, true);
        rdl->AddText({rowsOrigin.x + 6.0f, y + (rowH - ImGui::GetTextLineHeight()) * 0.5f}, ToU32(exists ? Theme::kText : Theme::kWarning), label.c_str());
        rdl->PopClipRect();
        ImGui::SetCursorScreenPos({rowsOrigin.x, y});
        ImGui::InvisibleButton("##label", {labelW, rowH});
        if (ImGui::IsItemClicked()) {
            if (Widget* tw = asset.root->Find(t.widget)) {
                ui.selection = PathOf(tw);
                ui.hasSelection = true;
            }
        }
        if (!exists && ImGui::IsItemHovered()) ImGui::SetTooltip("このウィジェットはもうありません（名前を変えたか消した）");
        if (ImGui::BeginPopupContextItem("track-menu")) {
            if (ImGui::MenuItem(ICON_FA_DIAMOND "  今の時刻にキーを打つ（今の値）")) {
                float v = t.curve.Evaluate(ui.animTime);
                if (Widget* dw = doc.view ? doc.view->Find(t.widget) : nullptr) WidgetProperties::Get(*dw, t.curve.property, v);
                t.curve.SetKey(ui.animTime, v);
                doc.Changed();
            }
            if (ImGui::MenuItem(ICON_FA_TRASH "  トラックを削除")) removeTrack = ti;
            ImGui::EndPopup();
        }
        // キー
        ImGui::SetCursorScreenPos({rowsOrigin.x + labelW, y});
        ImGui::InvisibleButton("##keys", {std::max(area.x - labelW, 1.0f), rowH});
        const bool rowHovered = ImGui::IsItemHovered();
        int hoverKey = -1;
        for (int ki = 0; ki < static_cast<int>(t.curve.keys.size()); ++ki) {
            const float kx = timeToX(t.curve.keys[static_cast<size_t>(ki)].time);
            if (rowHovered && std::abs(io.MousePos.x - kx) <= 6.0f) hoverKey = ki;
        }
        if (ImGui::IsItemClicked()) {
            if (hoverKey >= 0) {
                ui.keySelected = true;
                ui.keyTrack = ti;
                ui.keyTime = t.curve.keys[static_cast<size_t>(hoverKey)].time;
                ui.keyDragging = true;
                ui.animTime = ui.keyTime;
            } else {
                ui.keySelected = false;
                ui.animTime = snap(xToTime(io.MousePos.x));
            }
            ui.animPlaying = false;
        }
        if (ImGui::IsItemActive() && ui.keyDragging && ui.keySelected && ui.keyTrack == ti && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
            const float nt = snap(xToTime(io.MousePos.x));
            const int ki = t.curve.FindKey(ui.keyTime);
            if (ki >= 0 && std::abs(nt - ui.keyTime) > 1e-5f && t.curve.FindKey(nt) < 0) {
                t.curve.keys[static_cast<size_t>(ki)].time = nt;
                t.curve.Sort();
                ui.keyTime = nt;
                ui.animTime = nt;
                doc.Changed();
            }
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) ui.keyDragging = false;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && hoverKey >= 0) {
            ui.keySelected = true;
            ui.keyTrack = ti;
            ui.keyTime = t.curve.keys[static_cast<size_t>(hoverKey)].time;
            ImGui::OpenPopup("key-menu");
        }
        if (ImGui::BeginPopup("key-menu")) {
            const int ki = t.curve.FindKey(ui.keyTime);
            if (ki >= 0) {
                TimelineKey& k = t.curve.keys[static_cast<size_t>(ki)];
                ImGui::TextDisabled("キー %.2f 秒", k.time);
                ImGui::SetNextItemWidth(120.0f * s);
                if (ImGui::DragFloat("値", &k.value, 0.01f)) doc.Changed();
                const char* names[] = {"スムーズ", "リニア", "コンスタント"};
                for (int i = 0; i < 3; ++i) {
                    if (ImGui::MenuItem(names[i], nullptr, static_cast<int>(k.interpolation) == i)) {
                        k.interpolation = static_cast<KeyInterpolation>(i);
                        doc.Changed();
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_TRASH "  削除", "Delete")) {
                    t.curve.keys.erase(t.curve.keys.begin() + ki);
                    ui.keySelected = false;
                    doc.Changed();
                }
            }
            ImGui::EndPopup();
        }
        // 線とキーを描く
        if (t.curve.keys.size() >= 2)
            rdl->AddLine({timeToX(t.curve.keys.front().time), y + rowH * 0.5f}, {timeToX(t.curve.keys.back().time), y + rowH * 0.5f}, Theme::U32(Theme::kAccent, 0.35f), 2.0f);
        for (int ki = 0; ki < static_cast<int>(t.curve.keys.size()); ++ki) {
            const float kx = timeToX(t.curve.keys[static_cast<size_t>(ki)].time);
            const float r = (ki == hoverKey ? 6.0f : 5.0f) * s;
            const bool ksel = ui.keySelected && ui.keyTrack == ti && std::abs(ui.keyTime - t.curve.keys[static_cast<size_t>(ki)].time) < 1e-5f;
            const ImU32 fill = ksel ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 200, 80, 255);
            const ImVec2 c{kx, y + rowH * 0.5f};
            rdl->AddQuadFilled({c.x, c.y - r}, {c.x + r, c.y}, {c.x, c.y + r}, {c.x - r, c.y}, fill);
            rdl->AddQuad({c.x, c.y - r}, {c.x + r, c.y}, {c.x, c.y + r}, {c.x - r, c.y}, IM_COL32(20, 20, 24, 255));
        }
        ImGui::PopID();
    }
    if (removeTrack >= 0) {
        anim->tracks.erase(anim->tracks.begin() + removeTrack);
        if (doc.view) doc.view->SetAsset(doc.asset);
        doc.Changed();
    }
    if (anim->tracks.empty()) {
        ImGui::SetCursorScreenPos({rowsOrigin.x + 6.0f, rowsOrigin.y + 6.0f});
        ImGui::TextDisabled("トラックがありません。録画（●）中に数値を変えるか、「+ トラック」で追加します");
    }
    ImGui::SetCursorScreenPos({rowsOrigin.x, rowsOrigin.y + static_cast<float>(anim->tracks.size()) * rowH});
    ImGui::Dummy({1.0f, 1.0f});
    // 再生ヘッド
    const float px = timeToX(ui.animTime);
    rdl->AddLine({px, ImGui::GetWindowPos().y}, {px, ImGui::GetWindowPos().y + ImGui::GetWindowHeight()},
                 ui.animRecording ? IM_COL32(240, 70, 70, 230) : IM_COL32(255, 255, 255, 200), 1.5f);
    ImGui::EndChild();
    dl->AddTriangleFilled({px - 5.0f, origin.y}, {px + 5.0f, origin.y}, {px, origin.y + rulerH}, ui.animRecording ? IM_COL32(240, 70, 70, 255) : ToU32(Theme::kAccent));
    ImGui::EndChild();
}

} // namespace AquaTool
