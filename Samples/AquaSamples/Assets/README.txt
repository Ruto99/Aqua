ゲームで使う画像（PNG / JPG / BMP / TGA）やフォントなどをこのフォルダに置いてください。
ビルドすると実行ファイルの隣の Assets フォルダにコピーされます。

コードからは次のように読み込めます。

    auto texture = Ruto::Texture::Load(Ruto::Application::GetAssetPath("player.png"));
