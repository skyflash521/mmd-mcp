# CLAUDE.md

このファイルはClaude Code (claude.ai/code) がこのリポジトリで作業する際のカスタム指示です。

## 基本ルール

- 会話・コメント・コミットメッセージなどはすべて日本語で行うこと（MCPのツール定義は除く）
- MCPのツール定義（name, description, エラーメッセージ等）は英語で記述すること
- ユーザーの指示が技術的に誤っている可能性がある場合、安易に実行せず先に指摘・確認すること
- コミット前にメッセージをテキストで提示し、承認を得てから実行すること

## ビルド

必要環境:
- CMake 4.1+
- MSVC (Visual Studio) C++20対応
- DirectX SDK (June 2010): `C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)`

```bash
# 構成 (初回 or CMakeLists.txt変更時)
cmake --preset local

# ビルドのみ
cmake --build build

# ビルド+デプロイ
cmake --build build --target deploy
```

出力: `build/Debug/mmd_mcp.dll`

デプロイ先は `CMakeUserPresets.json` の `MMD_PLUGIN_DIR` で設定する（ローカル設定、Git管理外）。

## 備考

- MMDPluginヘッダ (`include/mmd_plugin.h`) は変更しないこと。プロジェクト設定で対応する
- MMDPluginヘッダがC++20で削除された `std::result_of_t` を使用するため、CMakeLists.txtで `_HAS_DEPRECATED_RESULT_OF=1` を定義して復活させている
