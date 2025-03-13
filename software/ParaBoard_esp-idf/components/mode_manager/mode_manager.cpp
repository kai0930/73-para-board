#include "mode_manager.hpp"

#include <stdio.h>

ModeManager::ModeManager()
    : current_mode_(ModeCommand::START), is_initialized_(false) {}

ModeManager::~ModeManager() { end(); }

void ModeManager::begin() {
  if (!is_initialized_) {
    is_initialized_ = true;
    // 初期モード（START）のアクティベートコールバックを呼び出す
    auto it = activate_callbacks_.find(current_mode_);
    if (it != activate_callbacks_.end()) {
      it->second(current_mode_,
                 current_mode_);  // 初期状態では前のモードも現在のモードも同じ
    }
  }
}

void ModeManager::end() {
  if (is_initialized_) {
    // 現在のモードのディアクティベートコールバックを呼び出す
    auto it = deactivate_callbacks_.find(current_mode_);
    if (it != deactivate_callbacks_.end()) {
      it->second(
          current_mode_,
          current_mode_);  // 終了時は次のモードも現在のモードも同じとする
    }
    is_initialized_ = false;
  }
}

void ModeManager::setupMode(ModeCommand mode, ModeCallback activate_cb,
                            ModeCallback deactivate_cb) {
  // アクティベートコールバックを設定
  if (activate_cb) {
    activate_callbacks_[mode] = activate_cb;
  }

  // ディアクティベートコールバックを設定
  if (deactivate_cb) {
    deactivate_callbacks_[mode] = deactivate_cb;
  }
}

ModeCommand ModeManager::getMode() const { return current_mode_; }

bool ModeManager::changeMode(ModeCommand new_mode) {
  if (!is_initialized_) {
    return false;
  }

  if (current_mode_ == new_mode) {
    return true;  // 既に同じモードの場合は何もしない
  }

  // 現在のモードのディアクティベートコールバックを呼び出す
  auto deactivate_it = deactivate_callbacks_.find(current_mode_);
  if (deactivate_it != deactivate_callbacks_.end()) {
    deactivate_it->second(current_mode_, new_mode);
  }

  // モードを変更
  ModeCommand previous_mode = current_mode_;
  current_mode_ = new_mode;

  // 新しいモードのアクティベートコールバックを呼び出す
  auto activate_it = activate_callbacks_.find(new_mode);
  if (activate_it != activate_callbacks_.end()) {
    activate_it->second(previous_mode, new_mode);
  }

  return true;
}

std::string ModeManager::getModeString(ModeCommand mode) {
  switch (mode) {
    case ModeCommand::START:
      return "START";
    case ModeCommand::LOGGING:
      return "LOGGING";
    default:
      return "UNKNOWN";
  }
}
