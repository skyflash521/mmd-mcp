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

## MCP仕様準拠（サーバー側 MUST チェックリスト）

このリポジトリでMCPサーバー実装を変更する際、以下の **MUST** を満たさない変更はマージしない。

### 1. ライフサイクル / 初期化
- サーバーは `initialize` 要求に対し、`result.protocolVersion` を返すこと（MUST）。
- サーバーが要求された `protocolVersion` をサポートしている場合、同じ値を返すこと（MUST）。
- サーバーが要求された `protocolVersion` をサポートしていない場合、サポートしている別の `protocolVersion` を返すこと（MUST）。
- `initialize` 応答に `capabilities` と `serverInfo` を含めること（MUST）。

### 2. JSON-RPC 基本要件
- JSON-RPC 2.0 形式（`jsonrpc: "2.0"`）を満たす要求に対し、`id` 付き要求には対応する応答を返すこと（MUST）。
- 不正リクエストには JSON-RPC error を返すこと（MUST）。
- `id` なし通知は JSON-RPC 応答ボディを返さないこと（MUST）。

### 3. Tools 機能（提供する場合）
- `initialize.result.capabilities.tools` を宣言すること（MUST）。
- `tools/list` で各ツールの `name` `description` `inputSchema` を返すこと（MUST）。
- `tools/call` の成功結果は `result` で返すこと（MUST）。
- 未知ツールや不正パラメータは JSON-RPC error で返すこと（MUST）。
- tools/call の要求形式が正しい場合は result を返し、ツール実行結果が失敗のときは result.isError: true を設定すること（MUST）。

### 4. Streamable HTTP（HTTP transportを実装する場合）
- MCPエンドポイントで `POST` を受理すること（MUST）。
- `GET` で `text/event-stream` を提供しない場合は `405 Method Not Allowed` を返すこと（MUST）。
- 通知/レスポンス入力を受理した場合は `202 Accepted`（ボディなし）を返すこと（MUST）。
- `MCP-Protocol-Version` が無効または非対応の場合は `400` を返すこと（MUST）。
- セッション管理を採用する場合、終了済みセッションID付き要求に `404` を返すこと（MUST）。

### 5. セキュリティ（HTTP）
- `Origin` 検証を実装し、不正Originには `403` を返すこと（MUST）。
- ローカル運用時は `127.0.0.1` バインドを優先すること（MUST）。

### 6. 実装・レビュー運用ルール
- MCP仕様の MUST を満たさない変更提案は、実装前に差し戻すこと（MUST）。
- 仕様判断が曖昧な場合は MCP 2025-11-25 の該当節を確認してから実装すること（MUST）。
- 互換モードを入れる場合は、デフォルト動作と逸脱理由を明記すること（MUST）。
