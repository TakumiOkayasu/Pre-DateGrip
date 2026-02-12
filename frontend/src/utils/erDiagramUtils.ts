import { DEFAULT_PAGE } from './erDiagramConstants';

/** テーブル一覧からユニークなページ名を抽出 */
export function extractPages(tables: { data: { page?: string } }[]): string[] {
  const pageSet = new Set<string>();
  for (const t of tables) {
    pageSet.add(t.data.page || DEFAULT_PAGE);
  }
  return Array.from(pageSet);
}
