# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Pre-DateGrip is a Windows-only high-performance RDBMS management tool with DataGrip-like UI/UX, targeting SQL Server as the primary database.

## Build Commands

**重要**: ビルドには必ず `uv run scripts/pdg.py` のCLIを使用すること。直接 `cmake`, `bun`, `npm` 等を実行しないこと。

All build scripts are Python-based (requires Python 3.14+) and auto-detect MSVC environment.
Scripts use [uv](https://docs.astral.sh/uv/) for execution (install: `winget install astral-sh.uv`).

### Unified CLI (推奨)

```bash
# Build commands
uv run scripts/pdg.py build backend              # Build C++ backend (Release)
uv run scripts/pdg.py build backend --clean      # Clean build backend
uv run scripts/pdg.py build backend --type Debug # Debug build
uv run scripts/pdg.py build frontend             # Build frontend (Bun/npm)
uv run scripts/pdg.py build frontend --clean     # Clean build frontend
uv run scripts/pdg.py build all                  # Build both (frontend + backend)

# Short aliases
uv run scripts/pdg.py b backend                  # Same as 'build backend'
uv run scripts/pdg.py b all --clean              # Clean build all

# Test commands
uv run scripts/pdg.py test frontend              # Run frontend tests
uv run scripts/pdg.py test frontend --watch      # Run tests in watch mode
uv run scripts/pdg.py test backend               # Run C++ tests (Release)
uv run scripts/pdg.py test backend --type Debug  # Run C++ tests (Debug)

# Short aliases
uv run scripts/pdg.py t frontend                 # Same as 'test frontend'

# Lint commands
uv run scripts/pdg.py lint                       # Lint all (frontend + C++)
uv run scripts/pdg.py lint --fix                 # Auto-fix lint issues

# Short alias
uv run scripts/pdg.py l --fix                    # Same as 'lint --fix'

# Development server
uv run scripts/pdg.py dev                        # Start frontend dev server (localhost:5173)

# Package for distribution
uv run scripts/pdg.py package                    # Build + package for release

# Comprehensive check
uv run scripts/pdg.py check Release              # Lint + test + build (all checks)

# Help
uv run scripts/pdg.py --help                     # Show all commands
uv run scripts/pdg.py build --help               # Show build options
```

### Legacy Scripts (後方互換性のために残存)

```bash
# These still work but pdg.py is recommended
uv run scripts/build_backend.py Release
uv run scripts/build_frontend.py
uv run scripts/test_frontend.py
uv run scripts/package.py
uv run scripts/convert_eol.py lf frontend/src   # EOL conversion (utility)
```

### ビルドスクリプト使用ガイドライン

**必ず pdg.py を使用する場合:**

- バックエンド (C++) のビルド → `uv run scripts/pdg.py build backend`
- バックエンドのテスト → `uv run scripts/pdg.py test backend`
- フロントエンドのビルド（本番用） → `uv run scripts/pdg.py build frontend`
- パッケージング → `uv run scripts/pdg.py package`
- 全体チェック → `uv run scripts/pdg.py check Release`
- Lint → `uv run scripts/pdg.py lint`

**統一CLI（推奨）:**

- 開発サーバー: `uv run scripts/pdg.py dev`
- テスト: `uv run scripts/pdg.py test frontend`
- Lint: `uv run scripts/pdg.py lint`
- ビルド: `uv run scripts/pdg.py build all`

**直接実行が許可される場合（オプション）:**

- フロントエンド開発サーバー: `cd frontend && bun run dev`
- フロントエンドのテスト: `cd frontend && bun run test`
- フロントエンドのLint: `cd frontend && bun run lint`

**理由:**

- ビルドスクリプトはMSVC環境を自動検出・設定
- インクリメンタルビルドとクリーンビルドの管理
- Precompiled Headers (PCH) と ccache の最適化を適用
- Bunの存在確認と依存関係の自動インストール
- **ビルド後にWebView2キャッシュを自動削除**（フロントエンドの変更を確実に反映）
- CI/CD環境との一貫性を保証
- プロジェクトルートからの統一的な実行方法

**CI/CD環境での例外:**

- GitHub ActionsなどのCI環境では、直接 `cmake` や `bun run build` を実行してもOK
- CI環境では環境変数やキャッシュが適切に管理されているため

### VSCode CMake Tools使用時の注意事項

**重要**: VSCodeでCMake Toolsを使用する場合、必ず**Developer Command Prompt for VS 2022**からVSCodeを起動してください。

#### 正しい起動方法

1. **スタートメニュー** → **Visual Studio 2022** → **Developer Command Prompt for VS 2022** を起動
2. プロンプトで以下を実行:

   ```cmd
   cd D:\prog\Pre-DateGrip
   code .
   ```

これにより、MSVC環境変数（`INCLUDE`, `LIB`, `PATH`等）が正しく設定された状態でVSCodeが起動します。

#### CMake Presetsの使用

- `CMakePresets.json`でNinjaジェネレータとMSVCコンパイラ（cl.exe）を指定済み
- VSCode CMake Toolsは自動的にpresetsを認識
- コマンドパレット（Ctrl+Shift+P）から：
  - `CMake: Select Configure Preset` → `debug` または `release`
  - `CMake: Configure`
  - `CMake: Build`

#### トラブルシューティング

通常のコマンドプロンプトやPowerShellからVSCodeを起動すると、以下のエラーが発生します：

```text
CMake Error: Generator Visual Studio 17 2022 could not find any instance of Visual Studio.
```

または

```text
lld-link: error: could not open 'msvcrtd.lib': no such file or directory
```

**解決方法**: Developer Command Promptから起動し直すか、`uv run scripts/build_backend.py`を使用してください。

---

**Ninja Permission Error（自動回復機能）:**

ビルド中に以下のエラーが発生することがあります：

```text
ninja: error: failed recompaction: Permission denied
```

このエラーは `build.ninja` ファイルがロックされている場合に発生します。

**自動回復の仕組み:**

- `build_backend.py` が自動的にエラーを検出
- ビルドディレクトリの削除を最大3回リトライ（各試行の間に2秒待機）
- 成功したら自動的に CMake Configure を再実行
- **手動でコマンドを実行する必要はありません**

成功時のメッセージ例：

```text
[!] Detected Ninja permission error (build.ninja lock)
    Attempting auto-recovery...
    Removing build directory: C:\prog\Pre-DateGrip\build
    [OK] Build directory removed (attempt 1)
    [OK] Build directory recreated
    Retrying CMake configuration...
```

**自動回復を成功させるために:**

リトライ中にビルドディレクトリがロックされている場合、以下を確認してください：

- VSCode や Visual Studio でプロジェクトが開かれている → 閉じる
- PreDateGrip.exe が実行中 → 終了する
- タスクマネージャーで ninja.exe や cl.exe が実行中 → 終了する

確認後、スクリプトが自動的に再試行します。3回のリトライ後も失敗する場合は、上記を確認してから **ビルドスクリプトを再実行** してください：

```bash
uv run scripts/build_backend.py Release
```

**重要:** 手動でビルドディレクトリを削除したり、直接コマンドを実行したりする必要はありません。すべてビルドスクリプトが自動処理します。

---

**フロントエンドのデバッグ・トラブルシューティング:**

UI の問題（テーブルが表示されない、ローディングが終わらない等）が発生した場合：

**重要: ログの解析は Claude の仕事です。**
- ユーザーは問題を報告するだけでOK（例: "テーブルが表示されない"）
- Claude が自動的に `log/frontend.log` と `log/backend.log` を読み取り、分析
- ユーザーがログを手動で確認・解析する必要はありません

1. **ログファイルの場所:**
   ```bash
   # フロントエンドログ（Claude が自動解析）
   log/frontend.log

   # バックエンドログ（Claude が自動解析）
   log/backend.log
   ```

2. **重要なログパターン:**
   - `[QueryStore] openTableData called` - テーブルを開く操作開始
   - `[QueryStore] Column metadata loaded: X columns` - メタデータフェッチ完了
   - `[ResultGrid] columnDefs computed: X columns` - カラム定義生成完了
   - `[ResultGrid] onGridReady called` - AG Grid 初期化完了（**これが出ない場合は問題あり**）
   - `[ResultGrid] Fetching rows X - Y` - データフェッチ開始
   - `[ResultGrid] Fetched X rows in Yms` - データフェッチ完了

3. **よくある問題と解決方法:**

   **問題: テーブルが表示されない**
   - ログで `onGridReady called` が出力されているか確認
   - 出力されていない → AG Grid が初期化されていない
   - `columnDefs computed: 0 columns` → メタデータフェッチに失敗
   - バックエンドログでエラーがないか確認

   **問題: Loading が終わらない**
   - `Column metadata loaded` が出力されているか確認
   - 出力されている → フロントエンドの問題
   - 出力されていない → バックエンドの問題（接続、クエリ実行）

   **問題: データが表示されない**
   - `Fetching rows 0 - 100` が出力されているか確認
   - 出力されていない → データソースが設定されていない
   - 出力されているが `Fetched X rows` がない → バックエンドエラー

4. **WebView2 キャッシュのクリア:**

   フロントエンドの変更が反映されない場合：
   ```bash
   # ビルドスクリプトが自動的にキャッシュを削除
   uv run scripts/build_frontend.py

   # 手動で削除する場合（非推奨）
   Remove-Item -Recurse -Force build\Release\PreDateGrip.exe.WebView2
   Remove-Item -Recurse -Force build\Debug\PreDateGrip.exe.WebView2
   ```

5. **完全なクリーンビルド:**
   ```bash
   # フロントエンド
   uv run scripts/build_frontend.py --clean

   # バックエンド
   uv run scripts/build_backend.py Release --clean
   ```

## Current Status

All phases (0-7) are complete. The project includes:

### Backend (C++)

- ODBC SQL Server driver with connection pooling
- IPC handler with 40+ API routes
- Result caching (LRU) and async query execution
- SIMD-optimized filtering (AVX2)
- A5:ER file parser
- SQL formatter
- CSV/JSON/Excel exporters
- Settings and session persistence
- Global database object search
- **Automatic log file management** (cleared on app startup)

### Frontend (React/TypeScript)

- Monaco Editor integration
- AG Grid with virtual scrolling (**server-side row model** for large tables)
- Zustand state management
- ER diagram with React Flow
- Complete API bridge to backend

### Infrastructure

- GitHub Actions CI/CD (LLVM 21, Bun, Biome 2.3.8)
- Google Test for C++, Vitest for frontend
- Python build scripts with uv

## Technology Stack

### Backend (C++23)

- **Build**: CMake + Ninja (MSVC)
- **WebView**: webview/webview (OS WebView2)
- **Database**: ODBC Native API (SQL Server)
- **JSON**: simdjson
- **SQL Parser**: Custom implementation using modern C++23 patterns
- **Optimization**: SIMD (AVX2), Memory-Mapped Files
- **XML Parser**: pugixml (for A5:ER support)
- **Linter/Formatter**: clang-format, clang-tidy

### Frontend (React + TypeScript)

- **Runtime**: Bun (install: `powershell -c "irm bun.sh/install.ps1 | iex"`)
- **Build**: Vite
- **UI**: React 18
- **Editor**: Monaco Editor
- **Table**: AG Grid (Virtual Scrolling)
- **Styling**: CSS Modules + DataGrip dark theme
- **State**: Zustand
- **ER Diagram**: React Flow
- **Linter/Formatter**: Biome 2.3.8 (`winget install biomejs.biome`)

### Testing

- **C++**: Google Test
- **Frontend**: Vitest

### Development Tools

- **Git Hooks**: Husky (pre-commit)
- **EOL Normalization**: convert_eol.py (CRLF for Windows)

## Project Structure

```text
Pre-DateGrip/
├── src/                               # C++ Backend
│   ├── main.cpp
│   ├── webview_app.h/cpp
│   ├── ipc_handler.h/cpp
│   ├── database/
│   │   ├── sqlserver_driver.h/cpp
│   │   ├── connection_pool.h/cpp
│   │   ├── result_cache.h/cpp
│   │   ├── async_query_executor.h/cpp
│   │   ├── schema_inspector.h/cpp
│   │   ├── query_history.h/cpp
│   │   └── transaction_manager.h/cpp
│   ├── parsers/
│   │   ├── a5er_parser.h/cpp
│   │   └── sql_formatter.h/cpp
│   ├── exporters/
│   │   ├── csv_exporter.h/cpp
│   │   ├── json_exporter.h/cpp
│   │   └── excel_exporter.h/cpp
│   └── utils/
│       ├── json_utils.h/cpp
│       ├── simd_filter.h/cpp
│       ├── file_utils.h/cpp
│       ├── settings_manager.h/cpp
│       ├── session_manager.h/cpp
│       └── global_search.h/cpp
├── tests/                             # C++ Tests
├── frontend/                          # React Frontend
│   ├── src/
│   │   ├── api/bridge.ts              # IPC bridge to C++ backend
│   │   ├── store/                     # Zustand stores
│   │   ├── components/
│   │   │   ├── layout/                # MainLayout, LeftPanel, CenterPanel, BottomPanel
│   │   │   ├── editor/                # SqlEditor, EditorTabs, EditorToolbar
│   │   │   ├── grid/                  # ResultGrid, CellEditor
│   │   │   ├── tree/                  # ObjectTree, TreeNode, ContextMenu
│   │   │   ├── history/               # QueryHistory, HistoryItem
│   │   │   └── diagram/               # ERDiagram, TableNode
│   │   ├── hooks/
│   │   └── types/
│   └── tests/
└── third_party/                       # webview.h, simdjson.h, pugixml.hpp, xlsxwriter.h
```

## Development Guidelines

1. **TDD**: Write tests before implementation
2. **CI-first**: All commits must pass CI
3. **Phase order**: Implement Phase 1→7 sequentially
4. **UI/UX priority**: Faithfully reproduce DataGrip's UI/UX
5. **Error handling**: Check return values for all ODBC calls
6. **Memory management**: Follow RAII principles (use smart pointers)
7. **Performance**: Use Virtual Scrolling, SIMD, async processing

## Performance Targets

| Operation | Target |
|-----------|--------|
| App startup | < 0.3s |
| SQL Server connection | < 50ms |
| SELECT (1M rows) | < 500ms |
| Result display start | < 100ms |
| Virtual scroll | 60fps stable |
| SQL format | < 50ms |
| CSV export (100K rows) | < 2s |
| A5:ER load (100 tables) | < 1s |
| ER diagram render (50 tables) | < 500ms |
| Query history search (10K items) | < 100ms |

## Key Interfaces

### SchemaInspector (C++)

```cpp
class SchemaInspector {
public:
    std::vector<TableInfo> getTables(const std::string& database);
    std::vector<ColumnInfo> getColumns(const std::string& table);
    std::vector<IndexInfo> getIndexes(const std::string& table);
    std::vector<ForeignKeyInfo> getForeignKeys(const std::string& table);
    std::string generateDDL(const std::string& table);
};
```

### TransactionManager (C++)

```cpp
class TransactionManager {
public:
    void begin();
    void commit();
    void rollback();
    bool isInTransaction() const;
    TransactionState getState() const;
};
```

### Zustand Stores (TypeScript)

```typescript
interface ConnectionStore {
    connections: Connection[];
    activeConnectionId: string | null;
    addConnection: (conn: Connection) => void;
    removeConnection: (id: string) => void;
    setActive: (id: string) => void;
}

interface QueryStore {
    queries: Query[];
    activeQueryId: string | null;
    executeQuery: (id: string) => Promise<void>;
}

interface HistoryStore {
    history: HistoryItem[];
    addHistory: (item: HistoryItem) => void;
    searchHistory: (keyword: string) => HistoryItem[];
}
```

### ER Diagram Types (TypeScript)

```typescript
interface TableNode {
    id: string;
    type: 'table';
    data: { tableName: string; columns: Column[]; };
    position: { x: number; y: number };
}

interface RelationEdge {
    id: string;
    source: string;
    target: string;
    type: 'relation';
    data: { cardinality: '1:1' | '1:N' | 'N:M'; sourceColumn: string; targetColumn: string; };
}
```

## 重要な指示 (Instructions for Claude)

### Python実行について

- **Pythonスクリプトは必ず `uv run` 経由で実行すること**
- 例: `uv run scripts/build.py Debug`（`python scripts/build.py Debug` は使わない）
- uvがPython環境とインラインスクリプト依存を自動管理する

### 禁止事項

- **git commit, git push は絶対禁止**。コミットメッセージを考えるだけにすること。

### 変更時の必須確認事項

何か変更を加えた場合は、以下を必ず確認すること：

1. **ドキュメントの更新**
   - CLAUDE.md、README.md、コードコメント等の関連ドキュメントを更新
   - 新しいコマンド、オプション、機能を追加した場合は必ず記載
   - 削除・非推奨化した機能も明記

2. **CI/CDの確認**
   - `.github/workflows/` 内のワークフローファイルの更新が必要か確認
   - 新しい依存関係、ビルドステップ、テストコマンドを追加した場合は反映
   - CI でビルド・テストが通ることを確認

3. **後方互換性**
   - 既存のスクリプトやコマンドが動作し続けるか確認
   - 破壊的変更の場合は移行ガイドを CLAUDE.md に追加

### 作業完了時の必須チェック（コミット前に必ず実行）

作業が終わったら、以下のLinter/Formatterでエラーが出ないか**必ずチェック**すること：

```bash
# フロントエンド (Biome)
cd frontend && bun run lint

# C++ (clang-format) - 全ファイルをチェック
uv run scripts/cpp_check.py format

# または一括チェック（フロントエンド + C++）
uv run scripts/run_lint.py
```

**重要**: CIは Ubuntu 上で clang-format を実行するため、改行コード(CRLF/LF)の違いでエラーが出ることがある。ローカルで `clang-format -i` を実行してもCIで失敗する場合は、`.clang-format` の設定を確認すること。

### コーディング規約

#### フロントエンド (TypeScript/React)

- **Biomeの警告は必ず修正する**（--error-on-warnings で警告もエラー扱い）
- 非nullアサーション (`!`) は使用禁止 → 明示的なnullチェックを使用
- CSS Modulesを使用（グローバルCSSは避ける）
- Zustandでの状態管理

#### バックエンド (C++)

- **C++23**の機能を積極的に使用（std::expected, std::format, std::ranges等）
- **clang-format 21**でフォーマット（Google style base）
  - CI環境: LLVM 21 (apt.llvm.org)
  - ローカル: `winget install LLVM.LLVM`
  - **警告もエラーとして扱う** (`--Werror` フラグ使用)
  - CIでフォーマットチェックが通らない場合は `clang-format -i` で自動修正
- RAII原則に従う（スマートポインタ使用）
- ODBCの戻り値は必ずチェック
- **ヘッダーファイルのルール**:
  - C-Like（C互換）ヘッダー（`<intrin.h>`, `<malloc.h>`等）は標準C++ヘッダーで代替できる場合は使用しない
  - 例: `<intrin.h>` → `<immintrin.h>` (SIMD), `<malloc.h>` → `<cstdlib>`
  - Windows SDKヘッダー（`<Windows.h>`, `<sql.h>`等）は必要に応じて使用可

### 改行コード

- **CRLF (Windows)** で統一
- コミット前にHuskyが自動でconvert_eol.pyを実行
- 手動変換: `uv run scripts/convert_eol.py crlf`

### ビルドシステム

- **Ninja**を使用（Visual Studio generatorは使用しない）
- Developer Command Promptから実行（MSVCコンパイラが必要）
- 変数の型は基本的には`auto`を使用し、必要な時だけ特別に型を指定すること。

## Logging System

### ログファイルの場所

- **バックエンドログ**: `log/backend.log`
- **フロントエンドログ**: `log/frontend.log`

### 自動削除の仕組み

- **バックエンドログ**: アプリ起動時に自動削除（`logger.cpp` の `FileLogOutput` コンストラクタで `std::ios::trunc`）
- **フロントエンドログ**: 初回書き込み時に自動削除（`ipc_handler.cpp` の `writeFrontendLog` で static フラグ使用）

これにより、常に最新のログのみが保持され、デバッグが容易になります。

### WebView2キャッシュ管理

WebView2はHTML/JSファイルをキャッシュするため、フロントエンドの変更が反映されないことがあります。

**キャッシュの場所:**

- `build/Debug/PreDateGrip.exe.WebView2/`
- `build/Release/PreDateGrip.exe.WebView2/`

**自動削除:**

- `build_frontend.py` と `build_backend.py` はビルド後に自動でWebView2キャッシュを削除
- `--clean` フラグ使用時は、ビルド前にも削除

**手動削除:**

```bash
# PowerShell
Remove-Item -Recurse -Force build/Debug/PreDateGrip.exe.WebView2, build/Release/PreDateGrip.exe.WebView2
```

### Issue対応ワークフロー

#### 自動セキュリティスキャン

- **毎日 JST 00:00 (UTC 15:00)** に自動実行
- Semgrepがセキュリティ問題を検出すると自動でissueを作成
- ラベル: `semgrep`, `priority:high` or `priority:medium`

#### Issue対応の手順

1. **issueの取得と優先度確認**

   ```bash
   gh issue list --state open --json number,title,labels
   ```

2. **優先度順に対応**
   - `priority:critical` → 即座に対応
   - `priority:high` → 次に対応
   - `priority:medium` → 時間があれば対応
   - `priority:low` → 将来の改善

3. **修正完了後**
   - 修正内容をコミットメッセージにまとめる
   - issueをクローズ: `gh issue close <number> --comment "Fixed in commit <hash>"`

4. **Semgrep警告の抑制**
   - 意図的な`shell=True`使用など、安全が確認できる場合:

     ```python
     # nosemgrep: python.lang.security.audit.subprocess-shell-true
     result = subprocess.run(cmd, shell=True)  # Safe: hardcoded paths only
     ```

   - セキュリティコメントで理由を明記すること
