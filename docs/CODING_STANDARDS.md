# コーディング規約

## Python (Build Scripts)

- **Ruff** で Lint + Format
- Python 3.14+ の型ヒントを使用
- pyproject.toml で設定管理
- 行長: 100文字
- インデント: スペース4個

## フロントエンド (TypeScript/React)

- Biome で Lint
- 非nullアサーション (`!`) 禁止 → 明示的なnullチェック
- CSS Modules 使用
- Zustand で状態管理

## バックエンド (C++)

- C++23 機能を使用 (std::expected, std::format, std::ranges)
- clang-format 21 で自動フォーマット
- RAII原則 (スマートポインタ)
- ODBC戻り値は必ずチェック
- 変数は基本 `auto` を使用

## 共通

- 改行コード: CRLF (Windows)
- Husky が自動変換
