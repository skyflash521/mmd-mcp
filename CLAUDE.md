# CLAUDE.md

このファイルはClaude Code (claude.ai/code) がこのリポジトリで作業する際のカスタム指示です。

## 基本ルール

- 会話・コメント・コミットメッセージなどはすべて日本語で行うこと（MCPのツール定義は除く）
- MCPのツール定義（name, description, エラーメッセージ等）は英語で記述すること
- ユーザーの指示が技術的に誤っている可能性がある場合、安易に実行せず先に指摘・確認すること
- コミット前にメッセージをテキストで提示し、承認を得てから実行すること
- コミットメッセージは `git diff` / `git status` の実際の差分のみに基づいて作成すること。直近の作業記憶や会話の文脈に引きずられず、差分に存在しない変更を含めないこと

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

## テスト

### ユニットテスト

```bash
cmake --build build && build/Debug/test_common.exe && build/Debug/test_mcp.exe && build/Debug/test_camera_tools.exe && build/Debug/test_model_tools.exe && build/Debug/test_morph_tools.exe && build/Debug/test_timeline_tools.exe
```

MMD不要。モックを使用するため単体で実行可能。

### 結合テスト (test_integration)

```bash
build/Debug/test_integration.exe
```

**前提条件（実行前に必ず確認）:**
- MMDが起動済みで、mmd_mcp.dll がプラグインとしてロードされていること
- MCPサーバーが `127.0.0.1:3939` でリッスン中であること
- PMXモデルが1体以上ロードされていること（`list_models` やモーフ系テストの検証に必要）
- カメラキーフレームのフレーム9000〜9099が空いていること（テスト用フレーム範囲）

**実行手順:** 結合テストを実行する際は、必ず先に上記の前提条件をユーザーに提示し、準備完了の確認を得てから実行すること。前提条件を提示せずにいきなりテストを実行してはならない。

## 備考

- MMDPluginヘッダ (`include/mmd_plugin.h`) は変更しないこと。プロジェクト設定で対応する
- MMDPluginヘッダがC++20で削除された `std::result_of_t` を使用するため、CMakeLists.txtで `_HAS_DEPRECATED_RESULT_OF=1` を定義して復活させている
- PMX関連の機能を実装する際は `docs/PMX仕様.txt`（極北P作成の公式仕様書）を参照すること

## MCP仕様準拠（MUST）

このリポジトリでMCPサーバー実装を変更する際、以下の仕様MUSTを満たさない変更はマージしない。

### 1. ライフサイクル / 初期化
- サーバーは `initialize` 要求に対し、`result.protocolVersion` を返すこと（MUST）。
- サーバーが要求された `protocolVersion` をサポートしている場合、同じ値を返すこと（MUST）。
- サーバーが要求された `protocolVersion` をサポートしていない場合、サポートしている別の `protocolVersion` を返すこと（MUST）。
- `initialize` 応答に `capabilities` と `serverInfo` を含めること（MUST）。

参照: https://modelcontextprotocol.io/specification/2025-11-25/basic/lifecycle

### 2. JSON-RPC 基本要件
- JSON-RPC 2.0 形式（`jsonrpc: "2.0"`）を満たす要求に対し、`id` 付き要求には対応する応答を返すこと（MUST）。
- 不正リクエストには JSON-RPC error を返すこと（MUST）。
- `id` なし通知は JSON-RPC 応答ボディを返さないこと（MUST）。

### 3. Tools 機能（提供する場合）
- `initialize.result.capabilities.tools` を宣言すること（MUST）。
- `tools/list` で各ツールの `name` `description` `inputSchema` を返すこと（MUST）。
- `tools/call` の成功結果は `result` で返すこと（MUST）。
- 要求形式不正（未知ツール名、params欠落等）→ JSON-RPC error を返すこと（MUST）。
- 要求形式が正しいがツール実行が失敗した場合 → `result.isError: true` を設定すること（MUST）。

参照: https://modelcontextprotocol.io/specification/2025-11-25/server/tools

### 4. Streamable HTTP（HTTP transportを実装する場合）
- MCPエンドポイントで `POST` を受理すること（MUST）。
- `GET` で `text/event-stream` を提供しない場合は `405 Method Not Allowed` を返すこと（MUST）。
- 通知/レスポンス入力を受理した場合は `202 Accepted`（ボディなし）を返すこと（MUST）。
- `MCP-Protocol-Version` が無効値または非対応値の場合は `400` を返すこと（MUST）。ヘッダ欠落時の `400` は SHOULD（当プロジェクトでは後方互換のため緩和採用）。
- セッション管理を採用する場合、終了済みセッションID付き要求に `404` を返すこと（MUST）。

参照: https://modelcontextprotocol.io/specification/2025-11-25/basic/transports#streamable-http

### 5. セキュリティ（HTTP）
- `Origin` 検証を実装し、不正Originには `403` を返すこと（MUST）。
- ローカル運用時は `127.0.0.1` バインドを優先すること（MUST）。

## 運用方針（プロジェクト規約）

- MCP仕様の MUST を満たさない変更提案は、実装前に差し戻すこと。
- 仕様判断が曖昧な場合は MCP 2025-11-25 の該当節を確認してから実装すること。
- 互換モードを入れる場合は、デフォルト動作と逸脱理由を明記すること。
