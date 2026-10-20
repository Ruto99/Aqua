// アニメーション: 一覧・再生・録画・トラックとキー（Unreal の Widget Animation）

#include "AquaApp.h"
#include "AquaKeys.h"

#include <Ruto/EditorUI/EditorUI.h>
#include <Ruto/EditorUI/Widgets.h>

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <set>

namespace AquaTool {

using namespace Ruto::EditorUI;

void App::DrawAnimations(Document& doc) {
    WidgetAsset& asset = *doc.asset;
    DesignerState& ui = doc.ui;
    const float s = m_Scale;
    const ImGuiIO& io = ImGui::GetIO();

    if (ui.animation >= static_cast<int>(asset.animations.size())) ui.animation = -1;
    WidgetAnimation* anim = ui.animation >= 0 ? &asset.animations[static_cast<size_t>(ui.animation)] : nullptr;

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
        m_AnimTextMode = false;
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
                const bool changedAnim = ui.animation != i || m_AnimTextMode;
                ui.animation = changedAnim ? i : -1; // もう一度クリックで選択解除（アニメーションを見ない）
                m_AnimTextMode = false;
                ui.animTime = 0.0f;
                ui.animPlaying = false;
                ui.animKeys.Clear();
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
        ui.animKeys.Clear();
        if (doc.view) doc.view->SetAsset(doc.asset);
        doc.Changed();
    }
    if (asset.animations.empty()) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("フェードイン・ボタンの登場などを作れます");
        ImGui::PopTextWrapPos();
    }
    // 文字アニメーション（選んでいるテキストの 1 文字の動き。同じタイムラインで編集する）
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kAccent);
    ImGui::SeparatorText("文字アニメーション");
    ImGui::PopStyleColor();
    Widget* selWidget = ui.hasSelection ? Resolve(asset.root.get(), ui.selection) : nullptr;
    if (auto* text = dynamic_cast<UIText*>(selWidget)) {
        const bool revealUsed = text->reveal == TextReveal::Custom, effectUsed = text->effect == TextEffect::Custom;
        const std::string revealLabel = std::format(ICON_FA_FONT "  {}: 出し方{}", text->name, revealUsed ? "  " ICON_FA_CHECK : "");
        const std::string effectLabel = std::format(ICON_FA_FONT "  {}: 動き{}", text->name, effectUsed ? "  " ICON_FA_CHECK : "");
        if (ImGui::Selectable(revealLabel.c_str(), m_AnimTextMode && m_TextAnimSlot == 0)) {
            OpenTextAnimEditor(0);
            ui.animPlaying = false;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("文字が出るときの動き（1 文字ずつ順に動き出す）");
        if (ImGui::Selectable(effectLabel.c_str(), m_AnimTextMode && m_TextAnimSlot == 1)) {
            OpenTextAnimEditor(1);
            ui.animPlaying = false;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("ずっと繰り返す動き（<custom>…</custom> で一部だけにも使える）");
    } else {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("テキストを選ぶと、1 文字ずつの動きを作れます");
        ImGui::PopTextWrapPos();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ---- 文字アニメーション（同じパネルの右側で編集する） ----
    if (m_AnimTextMode) {
        ImGui::BeginChild("##anim-text", {0.0f, avail.y}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawTextAnimPanel(doc);
        ImGui::EndChild();
        return;
    }

    // ---- トラックとキー ----
    if (ui.animation >= static_cast<int>(asset.animations.size())) ui.animation = -1;
    anim = ui.animation >= 0 ? &asset.animations[static_cast<size_t>(ui.animation)] : nullptr;
    ImGui::BeginChild("##anim-tracks", {0.0f, avail.y}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollWithMouse);
    // 見出し（再生・逆再生・一時停止・録画・時刻）
    const float duration = anim ? anim->GetDuration() : 0.0f;
    // 再生 / 逆再生を始める（もう一度押すと一時停止。端にいたら反対の端から）
    auto play = [&](bool reverse) {
        if (ui.animPlaying && ui.animReverse == reverse) {
            ui.animPlaying = false;
            return;
        }
        if (!reverse && ui.animTime >= duration - 1e-3f) ui.animTime = 0.0f;
        if (reverse && ui.animTime <= 1e-3f) ui.animTime = duration;
        ui.animReverse = reverse;
        ui.animPlaying = true;
    };
    if (ui.preview && anim && doc.view) {
        // プレビュー中: ゲームと同じ再生（UIDocument の PlayAnimation・PlayAnimationReverse・PauseAnimation・ReverseAnimation）を試す
        WidgetDocument& view = *doc.view;
        const bool playing = view.IsAnimationPlaying(anim->name);
        const bool paused = view.IsAnimationPaused(anim->name);
        const bool reversed = view.IsAnimationReversed(anim->name);
        if (IconButton(ICON_FA_BACKWARD, "逆再生（PlayAnimationReverse。再生中なら ReverseAnimation で向きを変える）", playing && reversed && !paused)) {
            if (playing) {
                if (!reversed) view.ReverseAnimation(anim->name);
                view.ResumeAnimation(anim->name);
            } else {
                view.PlayAnimationReverse(anim->name, ui.animLoop);
            }
        }
        ImGui::SameLine(0.0f, 2.0f);
        if (IconButton(ICON_FA_PLAY, "再生（PlayAnimation。逆再生中なら ReverseAnimation で向きを変える）", playing && !reversed && !paused)) {
            if (playing) {
                if (reversed) view.ReverseAnimation(anim->name);
                view.ResumeAnimation(anim->name);
            } else {
                view.PlayAnimation(anim->name, ui.animLoop);
            }
        }
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::BeginDisabled(!playing);
        if (IconButton(ICON_FA_PAUSE, paused ? "続ける（ResumeAnimation）" : "一時停止（PauseAnimation）", paused)) {
            if (paused) view.ResumeAnimation(anim->name);
            else view.PauseAnimation(anim->name);
        }
        ImGui::SameLine(0.0f, 2.0f);
        if (IconButton(ICON_FA_STOP, "止める（StopAnimation。その時刻の状態のまま）")) view.StopAnimation(anim->name);
        ImGui::EndDisabled();
        ImGui::SameLine(0.0f, 2.0f);
        if (IconButton(ICON_FA_REPEAT, "繰り返す（次に再生するときの loop）", ui.animLoop)) ui.animLoop = !ui.animLoop;
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        if (playing) ImGui::TextColored(Theme::kSuccess, "%s %.2f / %.2f 秒", paused ? ICON_FA_PAUSE : reversed ? ICON_FA_BACKWARD : ICON_FA_PLAY, view.GetAnimationTime(anim->name), duration);
        else ImGui::TextDisabled("プレビュー中: ゲームと同じに再生して確かめる");
    } else {
        ImGui::BeginDisabled(!anim || ui.preview);
        if (IconButton(ICON_FA_BACKWARD_STEP, "最初へ")) {
            ui.animTime = 0.0f;
        }
        ImGui::SameLine(0.0f, 2.0f);
        const bool back = ui.animPlaying && ui.animReverse;
        if (IconButton(back ? ICON_FA_PAUSE : ICON_FA_BACKWARD, back ? "一時停止（Shift+Space）" : "逆再生（Shift+Space）", back)) play(true);
        ImGui::SameLine(0.0f, 2.0f);
        const bool fwd = ui.animPlaying && !ui.animReverse;
        if (IconButton(fwd ? ICON_FA_PAUSE : ICON_FA_PLAY, fwd ? "一時停止（Space）" : "再生（Space）", fwd)) play(false);
        ImGui::SameLine(0.0f, 2.0f);
        if (IconButton(ICON_FA_FORWARD_STEP, "最後へ")) {
            ui.animTime = duration;
            ui.animPlaying = false;
        }
        ImGui::SameLine(0.0f, 2.0f);
        if (IconButton(ICON_FA_REPEAT, "繰り返す（オフなら端で止まる）", ui.animLoop)) ui.animLoop = !ui.animLoop;
        ImGui::EndDisabled();
    }
    // Space で再生 / 一時停止、Shift+Space で逆再生（アニメーションのパネルを使っているとき）
    if (anim && !ui.preview && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() && !io.WantTextInput &&
        ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        play(io.KeyShift);
    }
    ImGui::SameLine(0.0f, 8.0f);
    ImGui::BeginDisabled(!anim || ui.preview);
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

    // 再生を進める（逆再生なら戻る。繰り返さないなら端で止まる）
    if (anim && ui.animPlaying && !ui.preview) {
        ui.animTime += io.DeltaTime * (ui.animReverse ? -1.0f : 1.0f);
        const float d = duration;
        if (d <= 1e-3f) {
            ui.animPlaying = false;
        } else if (ui.animTime > d || ui.animTime < 0.0f) {
            if (!ui.animLoop) {
                ui.animTime = std::clamp(ui.animTime, 0.0f, d);
                ui.animPlaying = false;
            } else if (ui.animTime > d) {
                ui.animTime = std::fmod(ui.animTime, d);
            } else {
                ui.animTime = d + std::fmod(ui.animTime, d);
            }
        }
    }
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
    // 区間（名前付きの範囲）を目盛りに帯で示す
    for (size_t si = 0; si < anim->sections.size(); ++si) {
        const WidgetAnimSection& sec = anim->sections[si];
        const ImU32 col = si % 2 ? Theme::U32(Theme::kCoral, 0.30f) : Theme::U32(Theme::kAccent, 0.30f);
        const float sx0 = timeToX(sec.start), sx1 = std::max(timeToX(sec.end), sx0 + 2.0f);
        dl->AddRectFilled({sx0, origin.y}, {sx1, origin.y + rulerH}, col, 3.0f);
        dl->PushClipRect({sx0, origin.y}, {sx1, origin.y + rulerH}, true);
        dl->AddText({sx0 + 4.0f, origin.y + 2.0f}, ToU32(Theme::kText), std::format("{}{}", sec.loop ? ICON_FA_REPEAT " " : "", sec.name).c_str());
        dl->PopClipRect();
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
    ImGui::BeginDisabled(!sel && asset.groups.empty());
    if (ImGui::SmallButton(ICON_FA_PLUS " トラック")) ImGui::OpenPopup("add-anim-track");
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::SmallButton(ui.curveView ? ICON_FA_LIST : ICON_FA_CHART_LINE)) {
        ui.curveView = !ui.curveView;
        if (ui.curveTrack < 0 && !anim->tracks.empty()) ui.curveTrack = ui.keySelected ? ui.keyTrack : 0;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("カーブ表示: トラックの値の変化をグラフで見て、キーを上下（値）・左右（時刻）に動かす");
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::SmallButton(ICON_FA_BOOKMARK)) ImGui::OpenPopup("anim-sections");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("区間: タイムラインの一部に名前を付け、スクリプトから PlayAnimation(\"区間の名前\") で再生する（In・Loop・Out など）");
    if (ImGui::BeginPopup("anim-sections")) {
        ImGui::TextDisabled("区間（名前で再生する範囲）");
        int removeSec = -1;
        for (size_t si = 0; si < anim->sections.size(); ++si) {
            WidgetAnimSection& sec = anim->sections[si];
            ImGui::PushID(static_cast<int>(si));
            ImGui::SetNextItemWidth(110.0f * s);
            if (ImGui::InputText("##name", &sec.name, ImGuiInputTextFlags_EnterReturnsTrue) || ImGui::IsItemDeactivatedAfterEdit()) doc.Changed();
            ImGui::SameLine();
            float range[2] = {sec.start, sec.end};
            ImGui::SetNextItemWidth(150.0f * s);
            if (ImGui::DragFloat2("##range", range, 0.01f, 0.0f, 600.0f, "%.2f")) {
                sec.start = std::max(range[0], 0.0f);
                sec.end = std::max(range[1], sec.start);
                doc.Changed();
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("繰り返す", &sec.loop)) doc.Changed();
            ImGui::SameLine();
            if (IconButton(ICON_FA_PLAY, "この区間を見る（始まりへ）")) ui.animTime = sec.start;
            ImGui::SameLine(0.0f, 2.0f);
            if (IconButton(ICON_FA_TRASH, "区間を削除")) removeSec = static_cast<int>(si);
            ImGui::PopID();
        }
        if (removeSec >= 0) {
            anim->sections.erase(anim->sections.begin() + removeSec);
            doc.Changed();
        }
        if (ImGui::Button(ICON_FA_PLUS " 区間を追加（今の時刻から最後まで）")) {
            WidgetAnimSection sec;
            sec.name = "Section";
            for (int n = 1; anim->FindSection(sec.name) || asset.FindAnimation(sec.name); ++n) sec.name = std::format("Section{}", n);
            sec.start = ui.animTime;
            sec.end = std::max(anim->GetDuration(), ui.animTime);
            anim->sections.push_back(sec);
            doc.Changed();
        }
        ImGui::EndPopup();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("選択中のウィジェットのプロパティを追加（今の値でキーを打つ）");
    if (ImGui::BeginPopup("add-anim-track")) {
        // グループのトラック（"@グループ名"。全員に同じ値を当てる）
        for (const WidgetGroup& g : asset.groups) {
            Widget* first = nullptr;
            for (const std::string& m : g.members) {
                if (!first) first = asset.root->Find(m);
            }
            if (!first) continue;
            if (ImGui::BeginMenu(std::format(ICON_FA_OBJECT_GROUP "  グループ {}", g.name).c_str())) {
                const std::string target = "@" + g.name;
                for (const auto& info : WidgetProperties::List(*first)) {
                    if (ShaderParamSlot(info.property) >= 0) continue;
                    const std::string label = PropertyLabelJa(info.property) + (info.channel.empty() ? "" : "." + info.channel);
                    if (ImGui::MenuItem(label.c_str(), nullptr, anim->FindTrack(target, info.path) != nullptr)) {
                        float v = 0.0f;
                        WidgetProperties::Get(*first, info.path, v);
                        WidgetAnimTrack* t = anim->FindTrack(target, info.path);
                        if (!t) {
                            anim->tracks.push_back({target, info.path, TimelineCurve{info.path, {}}});
                            t = &anim->tracks.back();
                        }
                        t->curve.SetKey(ui.animTime, v);
                        doc.Changed();
                    }
                }
                ImGui::EndMenu();
            }
        }
        if (!sel) {
            ImGui::EndPopup();
        } else {
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
    }

    // ---- カーブ表示 ----
    if (ui.curveView) {
        ImGui::SetCursorScreenPos({origin.x, origin.y + rulerH});
        ImGui::BeginChild("##anim-curve", {0.0f, 0.0f}, ImGuiChildFlags_None);
        if (ui.curveTrack >= static_cast<int>(anim->tracks.size())) ui.curveTrack = -1;
        // 左: トラックを選ぶ
        ImGui::BeginChild("##curve-tracks", {labelW, 0.0f}, ImGuiChildFlags_None);
        for (int ti = 0; ti < static_cast<int>(anim->tracks.size()); ++ti) {
            const WidgetAnimTrack& t = anim->tracks[static_cast<size_t>(ti)];
            if (ImGui::Selectable(std::format("{}  ·  {}##ct{}", t.widget, t.curve.property, ti).c_str(), ui.curveTrack == ti)) ui.curveTrack = ti;
        }
        if (anim->tracks.empty()) ImGui::TextDisabled("トラックがありません");
        ImGui::EndChild();
        ImGui::SameLine(0.0f, 0.0f);
        const ImVec2 g0 = ImGui::GetCursorScreenPos();
        const ImVec2 gs = ImGui::GetContentRegionAvail();
        ImDrawList* gdl = ImGui::GetWindowDrawList();
        gdl->AddRectFilled(g0, {g0.x + gs.x, g0.y + gs.y}, ToU32(Theme::kBackground));
        if (ui.curveTrack >= 0 && gs.x > 20.0f && gs.y > 20.0f) {
            WidgetAnimTrack& t = anim->tracks[static_cast<size_t>(ui.curveTrack)];
            // 値の範囲（キーとカーブから。上下に少し余白）
            float vmin = 1e30f, vmax = -1e30f;
            for (const TimelineKey& k : t.curve.keys) vmin = std::min(vmin, k.value), vmax = std::max(vmax, k.value);
            const float tEnd = std::max(anim->GetDuration(), xToTime(g0.x + gs.x));
            for (int i = 0; i <= 64; ++i) {
                const float v = t.curve.Evaluate(tEnd * static_cast<float>(i) / 64.0f);
                vmin = std::min(vmin, v), vmax = std::max(vmax, v);
            }
            if (vmin > vmax) vmin = 0.0f, vmax = 1.0f;
            if (vmax - vmin < 1e-3f) vmin -= 0.5f, vmax += 0.5f;
            const float pad = (vmax - vmin) * 0.12f;
            vmin -= pad, vmax += pad;
            auto valueToY = [&](float v) { return g0.y + gs.y - (v - vmin) / (vmax - vmin) * gs.y; };
            auto yToValue = [&](float y) { return vmin + (g0.y + gs.y - y) / gs.y * (vmax - vmin); };
            // 目盛り（値）
            for (int i = 0; i <= 4; ++i) {
                const float v = vmin + (vmax - vmin) * static_cast<float>(i) / 4.0f;
                const float y = valueToY(v);
                gdl->AddLine({g0.x, y}, {g0.x + gs.x, y}, Theme::U32(Theme::kBorder, 0.8f));
                gdl->AddText({g0.x + 4.0f, y - ImGui::GetTextLineHeight()}, ToU32(Theme::kTextDim), std::format("{:.2f}", v).c_str());
            }
            // カーブ
            ImVec2 prev{};
            for (int i = 0; i <= 240; ++i) {
                const float x = g0.x + gs.x * static_cast<float>(i) / 240.0f;
                const ImVec2 p{x, valueToY(t.curve.Evaluate(xToTime(x)))};
                if (i > 0) gdl->AddLine(prev, p, ToU32(Theme::kAccent), 2.0f);
                prev = p;
            }
            // キー（ドラッグで時刻と値を変える）
            ImGui::SetCursorScreenPos(g0);
            ImGui::InvisibleButton("##curve-area", gs, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
            const bool areaHovered = ImGui::IsItemHovered();
            int hoverKey = -1;
            for (int ki = 0; ki < static_cast<int>(t.curve.keys.size()); ++ki) {
                const TimelineKey& k = t.curve.keys[static_cast<size_t>(ki)];
                const ImVec2 c{timeToX(k.time), valueToY(k.value)};
                if (areaHovered && std::abs(io.MousePos.x - c.x) <= 7.0f && std::abs(io.MousePos.y - c.y) <= 7.0f) hoverKey = ki;
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                ui.curveKey = hoverKey;
                if (hoverKey < 0) ui.animTime = snap(xToTime(io.MousePos.x));
                ui.animPlaying = false;
            }
            if (ImGui::IsItemActive() && ui.curveKey >= 0 && ui.curveKey < static_cast<int>(t.curve.keys.size()) && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f)) {
                TimelineKey& k = t.curve.keys[static_cast<size_t>(ui.curveKey)];
                const float nt = snap(xToTime(io.MousePos.x));
                float nv = yToValue(io.MousePos.y);
                if (!io.KeyAlt) nv = std::round(nv * 100.0f) / 100.0f; // 0.01 刻み（Alt で自由）
                k.value = nv;
                if (std::abs(nt - k.time) > 1e-5f && t.curve.FindKey(nt) < 0) {
                    k.time = nt;
                    t.curve.Sort();
                    ui.curveKey = t.curve.FindKey(nt);
                }
                ui.animTime = k.time;
                doc.Changed();
            }
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) ui.curveKey = -1;
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && hoverKey >= 0) {
                ui.keySelected = true;
                ui.keyTrack = ui.curveTrack;
                ui.keyTime = t.curve.keys[static_cast<size_t>(hoverKey)].time;
                ImGui::OpenPopup("curve-key-menu");
            }
            if (ImGui::BeginPopup("curve-key-menu")) {
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
                    if (ImGui::MenuItem(ICON_FA_TRASH "  削除")) {
                        t.curve.keys.erase(t.curve.keys.begin() + ki);
                        doc.Changed();
                    }
                }
                ImGui::EndPopup();
            }
            for (int ki = 0; ki < static_cast<int>(t.curve.keys.size()); ++ki) {
                const TimelineKey& k = t.curve.keys[static_cast<size_t>(ki)];
                const ImVec2 c{timeToX(k.time), valueToY(k.value)};
                const float r = (ki == hoverKey || ki == ui.curveKey ? 7.0f : 5.5f) * s;
                gdl->AddQuadFilled({c.x, c.y - r}, {c.x + r, c.y}, {c.x, c.y + r}, {c.x - r, c.y}, ki == ui.curveKey ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 200, 80, 255));
                gdl->AddQuad({c.x, c.y - r}, {c.x + r, c.y}, {c.x, c.y + r}, {c.x - r, c.y}, IM_COL32(20, 20, 24, 255));
                if (ki == hoverKey) gdl->AddText({c.x + 8.0f, c.y - 18.0f}, ToU32(Theme::kText), std::format("{:.2f} 秒 · {:.2f}", k.time, k.value).c_str());
            }
            // 再生ヘッド
            const float hx = timeToX(ui.animTime);
            gdl->AddLine({hx, g0.y}, {hx, g0.y + gs.y}, IM_COL32(255, 255, 255, 200), 1.5f);
        } else {
            gdl->AddText({g0.x + 10.0f, g0.y + 10.0f}, ToU32(Theme::kTextDim), "左でトラックを選ぶと、値の変化を線で表示します（キーを上下左右にドラッグ）");
        }
        ImGui::EndChild();
        const float px = timeToX(ui.animTime);
        dl->AddTriangleFilled({px - 5.0f, origin.y}, {px + 5.0f, origin.y}, {px, origin.y + rulerH}, ui.animRecording ? IM_COL32(240, 70, 70, 255) : ToU32(Theme::kAccent));
        ImGui::EndChild();
        return;
    }

    // 行（トラック）: パネルの高さに入るよう行を詰めて、すべてのトラックを表示する
    ImGui::SetCursorScreenPos({origin.x, origin.y + rulerH});
    ImGui::BeginChild("##anim-rows", {0.0f, 0.0f}, ImGuiChildFlags_None);
    const ImVec2 rowsOrigin = ImGui::GetCursorScreenPos();
    ImDrawList* rdl = ImGui::GetWindowDrawList();
    const int trackCount = static_cast<int>(anim->tracks.size());
    const float trackH = trackCount > 0 ? std::clamp(std::floor(ImGui::GetContentRegionAvail().y / static_cast<float>(trackCount)), std::round(14.0f * s), rowH) : rowH;
    // キーの操作に使う行（トラックのカーブ）と、コピーしたキーを貼る先を探す名前
    std::vector<TimelineCurve*> rows;
    std::vector<std::string> rowIds;
    auto buildRows = [&]() {
        rows.clear();
        rowIds.clear();
        for (WidgetAnimTrack& t : anim->tracks) {
            rows.push_back(&t.curve);
            rowIds.push_back(t.widget + "\n" + t.curve.property);
        }
    };
    buildRows();
    KeySelection& ksel = ui.animKeys;
    if (!ksel.dragging) std::erase_if(ksel.keys, [&](const KeyRef& k) { return !FindSelectedKey(rows, k); });
    bool changed = false;
    int removeTrack = -1;
    for (int ti = 0; ti < trackCount; ++ti) {
        WidgetAnimTrack& t = anim->tracks[static_cast<size_t>(ti)];
        const float y = rowsOrigin.y + static_cast<float>(ti) * trackH;
        ImGui::PushID(ti);
        const bool isGroup = !t.widget.empty() && t.widget.front() == '@';
        const bool exists = isGroup ? asset.FindGroup(std::string_view(t.widget).substr(1)) != nullptr : asset.root->Find(t.widget) != nullptr;
        rdl->AddRectFilled({rowsOrigin.x, y}, {rowsOrigin.x + labelW, y + trackH}, ToU32(ti % 2 ? Theme::kPanel : Theme::kFrame));
        rdl->AddLine({rowsOrigin.x, y + trackH - 0.5f}, {rowsOrigin.x + area.x, y + trackH - 0.5f}, ToU32(Theme::kBorder));
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
        rdl->PushClipRect({rowsOrigin.x, y}, {rowsOrigin.x + labelW - 4.0f, y + trackH}, true);
        rdl->AddText({rowsOrigin.x + 6.0f, y + (trackH - ImGui::GetTextLineHeight()) * 0.5f}, ToU32(exists ? Theme::kText : Theme::kWarning), label.c_str());
        rdl->PopClipRect();
        ImGui::SetCursorScreenPos({rowsOrigin.x, y});
        ImGui::InvisibleButton("##label", {labelW, trackH});
        if (ImGui::IsItemClicked()) {
            if (Widget* tw = asset.root->Find(t.widget)) {
                ui.selection = PathOf(tw);
                ui.hasSelection = true;
            }
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) SelectAllKeys(rows, ksel, ti);
        }
        if (!exists && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", isGroup ? "このグループはもうありません" : "このウィジェットはもうありません（名前を変えたか消した）");
        if (isGroup && exists && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("グループのトラック: メンバー全員に同じ値を当てる");
        if (ImGui::BeginPopupContextItem("track-menu")) {
            if (ImGui::MenuItem(ICON_FA_DIAMOND "  今の時刻にキーを打つ（今の値）")) {
                float v = t.curve.Evaluate(ui.animTime);
                Widget* dw = nullptr;
                if (doc.view && isGroup) {
                    const auto members = doc.view->FindGroup(std::string_view(t.widget).substr(1));
                    if (!members.empty()) dw = members.front();
                } else if (doc.view) {
                    dw = doc.view->Find(t.widget);
                }
                if (dw) WidgetProperties::Get(*dw, t.curve.property, v);
                t.curve.SetKey(ui.animTime, v);
                ksel.Clear();
                ksel.Add(ti, ui.animTime);
                changed = true;
            }
            if (ImGui::MenuItem(ICON_FA_OBJECT_GROUP "  このトラックのキーをすべて選ぶ", nullptr, false, !t.curve.keys.empty())) SelectAllKeys(rows, ksel, ti);
            if (ImGui::MenuItem(ICON_FA_TRASH "  トラックを削除")) removeTrack = ti;
            ImGui::EndPopup();
        }
        // キーの間の線
        if (t.curve.keys.size() >= 2)
            rdl->AddLine({timeToX(t.curve.keys.front().time), y + trackH * 0.5f}, {timeToX(t.curve.keys.back().time), y + trackH * 0.5f}, Theme::U32(Theme::kAccent, 0.35f), 2.0f);
        ImGui::PopID();
    }

    // キー（選ぶ・まとめて動かす・囲んで選ぶ・ダブルクリックで打つ・右クリックでメニュー）
    KeyTimelineArgs args;
    args.rows = rows;
    args.rowH = trackH;
    args.pos = {rowsOrigin.x + labelW, rowsOrigin.y};
    args.width = std::max(area.x - labelW, 1.0f);
    args.timeToX = timeToX;
    args.xToTime = xToTime;
    args.snap = snap;
    args.playhead = &ui.animTime;
    args.playing = &ui.animPlaying;
    args.scale = s;
    const KeyTimelineResult kr = KeyTimelineArea("##anim-keys", args, ksel);
    changed |= kr.changed;
    if (kr.doubleClick && kr.row >= 0 && kr.row < trackCount) {
        // その時刻のカーブの値でキーを打つ
        WidgetAnimTrack& t = anim->tracks[static_cast<size_t>(kr.row)];
        t.curve.SetKey(kr.time, t.curve.keys.empty() ? 0.0f : t.curve.Evaluate(kr.time));
        ksel.Clear();
        ksel.Add(kr.row, kr.time);
        ui.animTime = kr.time;
        changed = true;
    }
    if (kr.keyMenu || kr.emptyMenu) {
        if (kr.emptyMenu) ui.animTime = kr.time; // 貼り付けはここに
        ui.animPlaying = false;
        ImGui::OpenPopup("anim-key-menu");
    }
    KeyShortcut action = KeyShortcut::None;
    const bool canPaste = m_KeyClipboard.kind == "widget" && !m_KeyClipboard.keys.empty();
    if (ImGui::BeginPopup("anim-key-menu")) {
        action = KeyMenuItems(rows, ksel, changed, canPaste);
        ImGui::EndPopup();
    }
    if (action == KeyShortcut::None && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) action = ReadKeyShortcut();
    // 貼り付け: コピーしたトラックに（無ければ作る）。1 つのトラックだけをコピーしてキーを選んでいれば、そのトラックに
    auto paste = [&](const KeyClipboard& clip, float at) {
        if (clip.kind != "widget" || clip.keys.empty()) return;
        std::set<std::string> ids;
        for (const KeyClip& k : clip.keys) ids.insert(k.id);
        const KeyRef* prim = ksel.Primary();
        const int fallback = ids.size() == 1 && prim ? prim->row : -1;
        if (fallback < 0) {
            for (const std::string& id : ids) {
                const size_t nl = id.find('\n');
                if (nl == std::string::npos) continue;
                const std::string widget = id.substr(0, nl), property = id.substr(nl + 1);
                const bool there = widget.starts_with("@") ? asset.FindGroup(std::string_view(widget).substr(1)) != nullptr : asset.root->Find(widget) != nullptr;
                if (there && !anim->FindTrack(widget, property)) anim->tracks.push_back({widget, property, TimelineCurve{property, {}}});
            }
            buildRows();
        }
        auto rowOf = [&](const std::string& id) {
            const auto it = std::find(rowIds.begin(), rowIds.end(), id);
            return it == rowIds.end() ? -1 : static_cast<int>(it - rowIds.begin());
        };
        changed |= PasteKeys(rows, clip, at, rowOf, fallback, ksel);
    };
    const float frame = 1.0f / 60.0f;
    switch (action) {
    case KeyShortcut::Copy: CopyKeys(rows, rowIds, ksel, m_KeyClipboard, "widget"); break;
    case KeyShortcut::Cut:
        CopyKeys(rows, rowIds, ksel, m_KeyClipboard, "widget");
        changed |= DeleteKeys(rows, ksel);
        break;
    case KeyShortcut::Paste: paste(m_KeyClipboard, ui.animTime); break;
    case KeyShortcut::Duplicate: {
        // 今の時刻に複製（今の時刻が元と同じなら、元のすぐ後ろに）
        float first = 0.0f, last = 0.0f;
        if (!SelectedRange(ksel, first, last)) break;
        KeyClipboard tmp;
        CopyKeys(rows, rowIds, ksel, tmp, "widget");
        const float at = std::abs(ui.animTime - first) < 1e-3f ? last + 6.0f * frame : ui.animTime;
        ksel.Clear(); // 元と同じトラックに貼る
        paste(tmp, at);
        break;
    }
    case KeyShortcut::Delete: changed |= DeleteKeys(rows, ksel); break;
    case KeyShortcut::SelectAll: SelectAllKeys(rows, ksel); break;
    case KeyShortcut::Deselect: ksel.Clear(); break;
    case KeyShortcut::NudgeLeft:
    case KeyShortcut::NudgeRight: {
        const float d = (action == KeyShortcut::NudgeLeft ? -frame : frame) * (io.KeyShift ? 10.0f : 1.0f);
        if (!ksel.Empty()) {
            changed |= MoveKeys(rows, ksel, d);
            if (const KeyRef* p = ksel.Primary()) ui.animTime = p->time;
        } else {
            ui.animTime = std::max(ui.animTime + d, 0.0f);
        }
        ui.animPlaying = false;
        break;
    }
    case KeyShortcut::PrevKey:
    case KeyShortcut::NextKey:
        ui.animTime = AdjacentKeyTime(rows, ui.animTime, action == KeyShortcut::NextKey ? 1 : -1);
        ui.animPlaying = false;
        break;
    case KeyShortcut::Home:
        ui.animTime = 0.0f;
        ui.animPlaying = false;
        break;
    case KeyShortcut::End:
        ui.animTime = anim->GetDuration();
        ui.animPlaying = false;
        break;
    default: break;
    }
    // 詳細の録画・カーブ表示が使う「今のキー」
    ui.keySelected = !ksel.Empty();
    if (const KeyRef* p = ksel.Primary()) {
        ui.keyTrack = p->row;
        ui.keyTime = p->time;
    }
    if (removeTrack >= 0) {
        anim->tracks.erase(anim->tracks.begin() + removeTrack);
        ksel.Clear();
        ui.keySelected = false;
        if (doc.view) doc.view->SetAsset(doc.asset);
        changed = true;
    }
    if (changed) doc.Changed();
    if (anim->tracks.empty()) {
        ImGui::SetCursorScreenPos({rowsOrigin.x + 6.0f, rowsOrigin.y + 6.0f});
        ImGui::TextDisabled("トラックがありません。録画（●）中に数値を変えるか、「+ トラック」で追加します");
    }
    ImGui::SetCursorScreenPos({rowsOrigin.x, rowsOrigin.y + static_cast<float>(trackCount) * trackH});
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
