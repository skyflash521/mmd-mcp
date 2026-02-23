# MMD MCP

MikuMikuDance (MMD) にMCP (Model Context Protocol) サーバー機能を追加するプラグイン。MMDPlugin (v0.41 x64) を使用。

## 使い方

### 1. MMDPlugin のインストール

MMD にプラグイン機構を追加する MMDPlugin が必要です。

[MMD Plugin Install Manager (PTOM76 fork)](https://github.com/PTOM76/MMDPluginInstallManager) を使用してインストールしてください。

### 2. mmd_mcp.dll のビルドと配置

下記の「ビルド」セクションに従って `mmd_mcp.dll` をビルドし、MMD の `Plugin/` ディレクトリ内にサブフォルダを作成して配置します。

```
MikuMikuDance/
  Plugin/
    mmd_mcp/
      mmd_mcp.dll
```

### 3. MMD を起動

MMD を起動すると、プラグインが自動的にロードされ、MCP サーバーが `http://127.0.0.1:3939` で待ち受けを開始します。

### 4. MCP クライアントの設定

Claude Code で以下のコマンドを実行して MCP サーバーを登録します。

```bash
claude mcp add mmd --transport streamable-http http://127.0.0.1:3939/mcp
```

他の MCP クライアントを使用する場合は、`http://127.0.0.1:3939/mcp` を Streamable HTTP エンドポイントとして設定してください。

> **注意:** MMD が起動していない状態では接続できません。必ず MMD を先に起動してからClaude Code を使用してください。

> **警告:** このプラグインは開発中であり、MMD のメモリを直接操作します。プロジェクトファイル（.pmm）やモデルファイル（.pmx）のバックアップを必ず取ってから使用してください。

## 依存

- CMake 4.1+
- MSVC (C++20)
- [DirectX SDK (June 2010)](https://www.microsoft.com/en-us/download/details.aspx?id=6812) — デフォルトパス以外の場合は環境変数 `DXSDK_DIR` を設定

## ビルド

```bash
cmake --preset default
cmake --build build
```

出力: `build/Debug/mmd_mcp.dll` を MMD の `Plugin/mmd_mcp/` ディレクトリに配置する。

## デプロイ

`CMakeUserPresets.json` で `MMD_PLUGIN_DIR` を設定:

```json
{
    "version": 6,
    "configurePresets": [
        {
            "name": "local",
            "inherits": "default",
            "cacheVariables": {
                "MMD_PLUGIN_DIR": "path/to/MikuMikuDance/Plugin/mmd_mcp"
            }
        }
    ]
}
```

構成してデプロイ:

```bash
cmake --preset local
cmake --build build --target deploy
```
