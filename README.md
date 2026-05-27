# MHXX Macro Controller

AndroidからMonster Hunter XX (Switch版) のマクロ操作を全自動化するツールです。  
Switch側はAtmosphère sysmoduleとして動作し、AndroidアプリからWiFi経由でボタン操作を受け取ります。

## ビルド状況

| | Status |
|---|---|
| Switch NSP | [![Build Switch](https://github.com/YOUR_NAME/mhxx-macro/actions/workflows/switch-build.yml/badge.svg)](https://github.com/YOUR_NAME/mhxx-macro/actions/workflows/switch-build.yml) |
| Android APK | [![Build Android](https://github.com/YOUR_NAME/mhxx-macro/actions/workflows/android-build.yml/badge.svg)](https://github.com/YOUR_NAME/mhxx-macro/actions/workflows/android-build.yml) |

## 必要な環境

### Switch 側
- Atmosphère 1.x 以上
- WiFiでAndroidと同じLANに接続

### Android 側
- Android 8.0 (API 26) 以上
- Switchと同じWiFiに接続

## セットアップ

### 1. Switch
1. `mhxx-macro.nsp` をSD カードの `/atmosphere/contents/XXXXXXXXXXXX/exefs.nsp` に配置
2. Switch を再起動
3. Switchの設定 → インターネット でIPアドレスを確認

### 2. Android
1. APKをインストール
2. アプリを起動 → SwitchのIPアドレスを入力
3. 「接続」ボタンをタップ

## 通信プロトコル

```
TCP Port: 8765

[CMD_LOAD  = 0x01] [step_count: u32 BE] [steps...]
[CMD_START = 0x02] [loop_count: u32 BE]  (0 = 無限)
[CMD_STOP  = 0x03]
[CMD_STATUS= 0x04] → [running: u8][loop_done: u32 BE][cur_step: u32 BE]

MacroStep (各 28 bytes, little-endian):
  u64 keys         // HidNpadButton ビットマスク
  s32 stick_lx
  s32 stick_ly
  s32 stick_rx
  s32 stick_ry
  u32 duration_ms  // 押し続ける時間
  u32 release_ms   // 離した後の待機時間
```

## ライセンス

MIT
