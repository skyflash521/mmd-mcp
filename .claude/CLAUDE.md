# プロジェクト概要

MikuMikuDance (MMD) にMCP (Model Context Protocol) サーバー機能を追加するプラグインDLL。MMDPlugin (v0.41 x64) を使用し、MMDが `MMDPlugin.dll` 経由でロードする。Direct3D9レンダリングパイプラインやウィンドウメッセージ処理にフックできる。

## アーキテクチャ

- **main.cpp** — プラグインエントリポイント。`create4()`/`destroy4()` をエクスポートし、`MyPlugin` のシングルトンインスタンス (`MMDPluginDLL4` 継承) を返す。`DllMain` も含む。
- **include/mmd_plugin.h** — MMDPluginヘッダ。プラグインインターフェース階層を定義:
  - `MMDPluginDLL1` — D3D9デバイスメソッドの呼び出し前コールバック (`BeginScene`, `DrawIndexedPrimitive` 等)
  - `MMDPluginDLL2` — D3D9の呼び出し後コールバック。結果を参照で受け取る (`PostBeginScene` 等)
  - `MMDPluginDLL3` — ライフサイクル (`start`/`stop`)、ウィンドウフック (`WndProc`, `KeyBoardProc`, `MouseProc`)、`getPluginTitle`
  - `MMDPluginDLL4` — D3DX9エフェクトファイルフック (`D3DXCreateEffectFromFileExW`)
- **lib/MMDPlugin/** — リンク用ビルド済み `MMDPlugin.lib` (CMakeLists.txtで明示的にリンク)

## MMDPlugin主要概念

- `MMDPluginDLL1`〜`MMDPluginDLL4` の仮想メソッドをオーバーライドして、D3D9呼び出しの前後をインターセプトする
- `mmp::getMMDMainData()` はハードコードされたメモリオフセット経由でMMD内部データ (カメラ、モデル、キーフレーム、入力状態) へのポインタを返す
- `mmp::WinAPIHooker<T>` はインポートテーブル書き換えにより任意のWin32 API関数をフックできる
- `mmp::MMDMainData` と `mmp::MMDModelData` はリバースエンジニアリングされたMMDメモリレイアウトで、`static_assert` によるオフセット検証付き
