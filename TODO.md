# TODO / Future Enhancements

このファイルはPre-DateGripの残タスクを管理します。

## 🚨 Claude Code向けルール 🚨

1. **作業開始前に必ずこのファイルを確認**
2. **タスク完了後は該当項目を削除または「完了済み」セクションに移動**
3. **新しいタスクが発生したらこのファイルに追記**

---

## 🔴 高優先度（UX向上に直結）

1. **ツリーノードのコンテキストメニュー**
   - 実装場所: `frontend/src/components/tree/TreeNode.tsx:151`
   - 内容: 右クリックで「SELECT文生成」「テーブル削除」「データエクスポート」など
   - 影響: DataGripライクなUX実現

## 🟡 中優先度（機能拡張）

1. **UIを本家DataGripに近づける** ([#56](https://github.com/TakumiOkayasu/Pre-DateGrip/issues/56))
   - 内容: カラースキーム、アイコン、レイアウトの改善
   - 継続的なタスク

2. **クエリ結果のソート・フィルタリング**
   - 実装場所: `frontend/src/components/results/ResultsTable.tsx`
   - 内容: カラムヘッダークリックでソート、フィルタ入力欄

3. **複数SQL文の個別実行**
   - 内容: エディタ内でカーソル位置の文のみ実行（Ctrl+Enter）
   - 現状: 全体実行のみ

## 🟢 低優先度（配布・運用フェーズ）

1. **インストーラーの用意** ([#34](https://github.com/TakumiOkayasu/Pre-DateGrip/issues/34))
   - 内容: WiXまたはInno Setupでインストーラー作成

2. **自動更新機能**
   - 内容: 新バージョンのチェック・自動更新

3. **エクスポート形式の拡張**
   - 現状: CSV, JSON, Excel対応
   - 追加: Markdown, HTML, SQL INSERT文

## 📋 技術的改善（パフォーマンス・品質）

1. **クエリキャンセル機能**
   - 内容: 長時間実行中のクエリを途中でキャンセル

2. **接続プールの最適化**
   - 実装場所: `backend/database/connection_pool.cpp`
   - 改善: タイムアウト設定、ヘルスチェック、接続再利用の改善

3. **エラーハンドリングの改善**
   - 内容: より詳細なエラーメッセージとリカバリー提案
   - 継続的なタスク

---

## ✅ 完了済み

- **SQLフォーマット** (Ctrl+Shift+F) - `backend/parsers/sql_formatter.cpp` で実装済み
- **SQLキーワード大文字変換** - Ctrl+Shift+Fでフォーマットと同時に実行
- **SQLキーワード外部ファイル化** - `config/sql_keywords.txt` で管理
- **複数SQL文の実行** - タブ式の結果表示で実装済み
- **インライン編集** - セル編集機能実装済み
- **履歴からのSQL実行機能** - ダブルクリックで実行 (b899ab6)
- **Ctrl+Wでタブを閉じる** - `MainLayout.tsx:233-235` で実装済み
- **クエリブックマーク機能** - クエリ保存/読み込み機能 (a579f1f)
