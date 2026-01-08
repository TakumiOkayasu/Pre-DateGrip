# ビルドコマンド

すべてのビルドは `uv run scripts/pdg.py` を使用。

**Requirements**: Python 3.14+, uv (`winget install astral-sh.uv`)

## 基本コマンド

```bash
# ビルド
uv run scripts/pdg.py build backend              # C++ (Release)
uv run scripts/pdg.py build backend --type Debug # C++ (Debug)
uv run scripts/pdg.py build backend --clean      # クリーンビルド
uv run scripts/pdg.py build frontend             # フロントエンド
uv run scripts/pdg.py build all                  # 全体ビルド

# テスト
uv run scripts/pdg.py test backend               # C++テスト
uv run scripts/pdg.py test frontend              # フロントエンドテスト
uv run scripts/pdg.py test frontend --watch      # Watchモード

# Lint
uv run scripts/pdg.py lint                       # 全体Lint (Frontend + C++)
uv run scripts/pdg.py lint --fix                 # 自動修正
ruff check scripts/ && ruff format scripts/      # Python (別途実行)

# 開発
uv run scripts/pdg.py dev                        # 開発サーバー (localhost:5173)

# その他
uv run scripts/pdg.py check Release              # 全チェック (lint + test + build)
uv run scripts/pdg.py package                    # リリースパッケージ作成
uv run scripts/pdg.py --help                     # ヘルプ表示
```

## ショートカット

`build` → `b`, `test` → `t`, `lint` → `l`, `dev` → `d`, `check` → `c`, `package` → `p`

```bash
uv run scripts/pdg.py b backend --clean
uv run scripts/pdg.py t frontend --watch
uv run scripts/pdg.py l --fix
```

## フロントエンド直接実行

```bash
cd frontend
bun run dev      # 開発サーバー
bun run test     # テスト
bun run lint     # Lint
```
