# プロジェクト概要

MikuMikuDance (MMD) にMCP (Model Context Protocol) サーバー機能を追加するプラグインDLL。MMDPlugin (v0.41 x64) を使用し、MMDが `MMDPlugin.dll` 経由でロードする。Direct3D9レンダリングパイプラインやウィンドウメッセージ処理にフックできる。

## アーキテクチャ

- **main.cpp** — プラグインエントリポイント。`version()`/`create4()`/`destroy4()` をエクスポートし、`MyPlugin` のシングルトンインスタンス (`MMDPluginDLL4` 継承) を返す。`create4()` でサーバー起動、`destroy4()` でサーバー停止。`DllMain` も含む。
- **mcp_server.h / mcp_server.cpp** — MCPサーバー。cpp-httplibを使用し、別スレッドで `127.0.0.1:3939` にHTTPサーバーを起動する。
- **include/mmd_plugin.h** — MMDPluginヘッダ。プラグインインターフェース階層を定義:
  - `MMDPluginDLL1` — D3D9デバイスメソッドの呼び出し前コールバック (`BeginScene`, `DrawIndexedPrimitive` 等)
  - `MMDPluginDLL2` — D3D9の呼び出し後コールバック。結果を参照で受け取る (`PostBeginScene` 等)
  - `MMDPluginDLL3` — ライフサイクル (`start`/`stop`)、ウィンドウフック (`WndProc`, `KeyBoardProc`, `MouseProc`)、`getPluginTitle`
  - `MMDPluginDLL4` — D3DX9エフェクトファイルフック (`D3DXCreateEffectFromFileExW`)
- **lib/MMDPlugin/** — リンク用ビルド済み `MMDPlugin.lib` (CMakeLists.txtで明示的にリンク)

## MMDPlugin主要概念

- `version()` は `4` を返す。`version()` の値に応じて `createN()`/`destroyN()` が呼ばれる
- version 4 では `MMDPluginDLL3::start()`/`stop()` はMMDPluginから自動呼び出しされない。`create4()` 内で `start()` を、`destroy4()` 内で `stop()` を手動で呼ぶ必要がある
- `MMDPluginDLL1`〜`MMDPluginDLL4` の仮想メソッドをオーバーライドして、D3D9呼び出しの前後をインターセプトする
- `mmp::getMMDMainData()` はハードコードされたメモリオフセット経由でMMD内部データ (カメラ、モデル、キーフレーム、入力状態) へのポインタを返す
- `mmp::WinAPIHooker<T>` はインポートテーブル書き換えにより任意のWin32 API関数をフックできる
- `mmp::MMDMainData` と `mmp::MMDModelData` はリバースエンジニアリングされたMMDメモリレイアウトで、`static_assert` によるオフセット検証付き
