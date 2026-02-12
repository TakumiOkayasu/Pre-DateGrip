/** A5:ER ファイルでPageプロパティが未設定時のデフォルトページ名 */
export const DEFAULT_PAGE = 'MAIN';

/** 「すべてのページ」を表す内部識別子 */
export const ALL_PAGES = '__ALL__';

/** ER図グリッド自動レイアウト設定 */
export const GRID_LAYOUT = {
  columns: 4,
  nodeWidth: 280,
  nodeHeight: 200,
  gap: 80,
} as const;
