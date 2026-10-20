#pragma once

// 編集用ファイル（AquaUI/*.aqs）の XML の読み書き
//
//   中身は Json と同じ木を、型の名前の要素で書いたもの（情報は失われず、キーの順も保たれる）。
//
//   <?xml version="1.0" encoding="UTF-8"?>
//   <AquaSource version="2" output="UI/Title.aqua" generator="Aqua 1.0.0">
//     <object key="document">
//       <array key="referenceResolution">
//         <number>1920</number>
//         <number>1080</number>
//       </array>
//       <object key="root">
//         <string key="type">CanvasPanel</string>
//         <bool key="visible">true</bool>
//         ...
//       </object>
//     </object>
//     <object key="preview">
//       <object key="variables">
//         <string key="score">1200</string>
//       </object>
//     </object>
//   </AquaSource>
//
//   ・要素は object / array / string / number / bool / null。オブジェクトの中では key 属性がキー
//   ・文字列の改行・タブなどは文字参照（&#10; など）にするので、1 つの値は必ず 1 行に収まる（差分が読みやすい）
//   ・ルートの属性 version・output・generator は Json の同じ名前のキー（format は "AquaSource"）

#include <Ruto/Core/Json.h>

#include <optional>
#include <string>
#include <string_view>

namespace AquaTool {

// .aqs の Json（format・version・output・generator・document・preview）→ XML の文字列
std::string SourceToXml(const Ruto::Json& source);
// XML の文字列 → .aqs の Json（ルートが <AquaSource> でなければ失敗）
std::optional<Ruto::Json> SourceFromXml(std::string_view text, std::string* error = nullptr);

// 任意の Json の値 ⇔ XML の要素（テスト・ほかの道具用）
std::string JsonToXmlElement(const Ruto::Json& value, int indent = 2);
std::optional<Ruto::Json> JsonFromXmlElement(std::string_view text, std::string* error = nullptr);

} // namespace AquaTool
