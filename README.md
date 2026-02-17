# MMD MCP

MikuMikuDance (MMD) にMCP (Model Context Protocol) サーバー機能を追加するプラグイン。MMDPlugin (v0.41 x64) を使用。

## 依存

- CMake 4.1+
- MSVC (C++20)
- [DirectX SDK (June 2010)](https://www.microsoft.com/en-us/download/details.aspx?id=6812) — デフォルトパス以外の場合は環境変数 `DXSDK_DIR` を設定

## ビルド

```bash
cmake --preset default
cmake --build build
```

出力: `build/Debug/mmd_mcp.dll` をMMDの `Plugin/` ディレクトリに配置する。

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
