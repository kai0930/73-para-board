# Mode Manager

モード管理のためのコンポーネントです。異なるモード間の遷移と、モード変更時のコールバック処理を管理します。

## 特徴

- 複数のモード（START、PREPARING、LOGGING）をサポート
- モード変更時のアクティベート/ディアクティベートコールバックをサポート
- 前のモードと次のモードの情報に基づいた処理が可能
- 再利用可能な設計で、他のプロジェクトでも使用可能

## 使用例

```cpp
#include "mode_manager.hpp"
#include <iostream>

void onStartActivate(Mode previous, Mode next) {
  std::cout << "STARTモードがアクティブになりました。前のモード: " 
            << ModeManager::getModeString(previous) << std::endl;
}

void onStartDeactivate(Mode previous, Mode next) {
  std::cout << "STARTモードが非アクティブになりました。次のモード: " 
            << ModeManager::getModeString(next) << std::endl;
}

void onPreparingActivate(Mode previous, Mode next) {
  std::cout << "PREPARINGモードがアクティブになりました。前のモード: " 
            << ModeManager::getModeString(previous) << std::endl;
  
  // 前のモードに応じた処理
  if (previous == Mode::START) {
    std::cout << "STARTモードから遷移しました。初期化処理を実行します。" << std::endl;
  } else if (previous == Mode::LOGGING) {
    std::cout << "LOGGINGモードから遷移しました。ログデータをクリアします。" << std::endl;
  }
}

int main() {
  ModeManager mode_manager;
  
  // モードごとのコールバックを設定
  mode_manager.setupMode(Mode::START, onStartActivate, onStartDeactivate);
  mode_manager.setupMode(Mode::PREPARING, onPreparingActivate, nullptr);
  
  // 初期化（STARTモードのアクティベートコールバックが呼ばれる）
  mode_manager.begin();
  
  // PREPARINGモードに変更（STARTのディアクティベートとPREPARINGのアクティベートが呼ばれる）
  mode_manager.changeMode(Mode::PREPARING);
  
  // 終了処理（現在のモードのディアクティベートコールバックが呼ばれる）
  mode_manager.end();
  
  return 0;
}
```

## カスタムモードの追加方法

他のプロジェクトで使用する場合は、`Mode`列挙型を拡張するか、独自のモード列挙型を定義して使用できます。

```cpp
// カスタムモードの例
enum class CustomMode {
  IDLE,
  ACTIVE,
  SLEEP
};

// カスタムモードマネージャー
using CustomModeCallback = std::function<void(CustomMode previous, CustomMode next)>;

class CustomModeManager {
  // ModeManagerと同様の実装...
};
``` 