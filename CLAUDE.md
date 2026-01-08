# CLAUDE.md

Pre-DateGrip固有の指示。グローバルルール (`~/.claude/CLAUDE.md`) に従った上で、以下を適用。

## プロジェクト概要

Windows専用RDBMS管理ツール（DataGripライクUI）。SQL Server対象。

- Backend: C++23 + ODBC + WebView2
- Frontend: React 18 + TypeScript + TanStack Table

## ビルド

```bash
uv run scripts/pdg.py build all      # 全体ビルド
uv run scripts/pdg.py lint --fix     # Lint修正
uv run scripts/pdg.py test frontend  # テスト
```

詳細: `docs/BUILD_COMMANDS.md`

## 作業完了時の必須チェック

```bash
uv run scripts/pdg.py lint                        # Frontend + C++
ruff check scripts/ && ruff format --check scripts/  # Python
```

## ドキュメント参照

必要に応じて以下を読み込んで情報を提供:

| ファイル | 内容 |
|----------|------|
| `TODO.md` | 残タスク一覧（作業前に確認） |
| `docs/PROJECT_INFO.md` | 技術スタック、構造、ショートカット |
| `docs/BUILD_COMMANDS.md` | ビルドコマンド詳細 |
| `docs/CODING_STANDARDS.md` | コーディング規約 |
| `docs/TROUBLESHOOTING.md` | トラブルシューティング |

## Claude Code責任範囲

### UI問題発生時

1. `log/frontend.log` と `log/backend.log` を確認
2. エラー原因を特定して修正
3. ユーザーは問題を報告するだけでOK
