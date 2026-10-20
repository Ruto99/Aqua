#include <Ruto/Ruto.h>

using namespace Ruto;

// 毎フレーム回転させるスクリプトの例。
// 新しいスクリプトはこのファイルをまねて Source フォルダに .cpp を追加し、
// エディターの「スクリプト再コンパイル」（Ctrl+R）を押すと「コンポーネントを追加」に出てくる。
class Rotator : public Component {
    RUTO_COMPONENT(Rotator)

    Vec3 speed{20.0f, 45.0f, 0.0f}; // 度/秒

    void Reflect(PropertyVisitor& v) override { v.Property("speed", speed); }

    void OnUpdate(float dt) override { GetTransform().rotation += speed * dt; }
};
RUTO_REGISTER_COMPONENT(Rotator);
