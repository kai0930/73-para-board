#pragma once

#include <functional>
#include <map>
#include <string>

#include "73CanConfig.hpp"

// モード遷移時のコールバック関数の型定義
using ModeCallback =
    std::function<void(ModeCommand previous_mode, ModeCommand next_mode)>;

class ModeManager {
 public:
  ModeManager();
  ~ModeManager();

  void begin();
  void end();

  // 特定のモードに対してアクティベート/ディアクティベートコールバックを設定
  void setupMode(ModeCommand mode, ModeCallback activate_cb,
                 ModeCallback deactivate_cb);

  // 現在のモードを取得
  ModeCommand getMode() const;

  // 新しいモードに切り替え
  bool changeMode(ModeCommand new_mode);

  // モード名を文字列として取得（デバッグ用）
  static std::string getModeString(ModeCommand mode);

 private:
  ModeCommand current_mode_;
  std::map<ModeCommand, ModeCallback> activate_callbacks_;
  std::map<ModeCommand, ModeCallback> deactivate_callbacks_;
  bool is_initialized_;
};