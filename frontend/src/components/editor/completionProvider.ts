import type * as Monaco from 'monaco-editor';
import { useSchemaStore } from '../../store/schemaStore';
import { log } from '../../utils/logger';

// SQLキーワード（基本的なもののみ）
const SQL_KEYWORDS = [
  'SELECT',
  'FROM',
  'WHERE',
  'JOIN',
  'LEFT',
  'RIGHT',
  'INNER',
  'OUTER',
  'ON',
  'AND',
  'OR',
  'NOT',
  'IN',
  'LIKE',
  'BETWEEN',
  'IS',
  'NULL',
  'ORDER',
  'BY',
  'GROUP',
  'HAVING',
  'INSERT',
  'INTO',
  'VALUES',
  'UPDATE',
  'SET',
  'DELETE',
  'CREATE',
  'ALTER',
  'DROP',
  'TABLE',
  'INDEX',
  'VIEW',
  'AS',
  'DISTINCT',
  'TOP',
  'LIMIT',
  'OFFSET',
  'UNION',
  'ALL',
  'CASE',
  'WHEN',
  'THEN',
  'ELSE',
  'END',
  'COUNT',
  'SUM',
  'AVG',
  'MIN',
  'MAX',
  'COALESCE',
  'ISNULL',
  'CAST',
  'CONVERT',
];

// エイリアス情報を解析
interface AliasInfo {
  alias: string;
  tableName: string;
}

function parseAliases(text: string): AliasInfo[] {
  const aliases: AliasInfo[] = [];

  // FROM table AS alias, FROM table alias, JOIN table AS alias, JOIN table alias
  const patterns = [
    /(?:FROM|JOIN)\s+\[?(\w+)\]?(?:\.\[?(\w+)\]?)?\s+(?:AS\s+)?(\w+)/gi,
    /(?:FROM|JOIN)\s+(\w+)(?:\.(\w+))?\s+(?:AS\s+)?(\w+)/gi,
  ];

  for (const pattern of patterns) {
    const matches = text.matchAll(pattern);
    for (const match of matches) {
      const schema = match[2] ? match[1] : null;
      const table = match[2] ?? match[1];
      const alias = match[3];
      if (alias && table && alias.toLowerCase() !== table.toLowerCase()) {
        aliases.push({
          alias: alias.toLowerCase(),
          tableName: schema ? `${schema}.${table}` : table,
        });
      }
    }
  }

  return aliases;
}

// カーソル位置の文脈を判断
type ContextType = 'table' | 'column' | 'alias_column' | 'keyword' | 'unknown';

interface CompletionContext {
  type: ContextType;
  aliasOrTable?: string;
}

function getCompletionContext(
  model: Monaco.editor.ITextModel,
  position: Monaco.Position
): CompletionContext {
  const lineContent = model.getLineContent(position.lineNumber);
  const textBeforeCursor = lineContent.substring(0, position.column - 1);

  // ドット直後: エイリアス.カラム または テーブル.カラム
  const dotMatch = textBeforeCursor.match(/(\w+)\.$/);
  if (dotMatch) {
    return { type: 'alias_column', aliasOrTable: dotMatch[1] };
  }

  // FROM/JOIN後: テーブル名補完
  if (/(?:FROM|JOIN)\s+$/i.test(textBeforeCursor)) {
    return { type: 'table' };
  }

  // SELECT, WHERE, ON, SET, AND, OR後: カラム名補完
  if (/(?:SELECT|WHERE|ON|SET|AND|OR|,)\s*$/i.test(textBeforeCursor)) {
    return { type: 'column' };
  }

  // デフォルト: キーワード
  return { type: 'keyword' };
}

export function createCompletionProvider(
  connectionId: string | null
): Monaco.languages.CompletionItemProvider {
  return {
    triggerCharacters: ['.', ' '],

    provideCompletionItems: async (
      model: Monaco.editor.ITextModel,
      position: Monaco.Position
    ): Promise<Monaco.languages.CompletionList> => {
      const suggestions: Monaco.languages.CompletionItem[] = [];
      const word = model.getWordUntilPosition(position);
      const range = {
        startLineNumber: position.lineNumber,
        endLineNumber: position.lineNumber,
        startColumn: word.startColumn,
        endColumn: word.endColumn,
      };

      const context = getCompletionContext(model, position);
      const fullText = model.getValue();
      const aliases = parseAliases(fullText);

      log.debug(`[CompletionProvider] Context: ${context.type}, ConnectionId: ${connectionId}`);

      if (context.type === 'alias_column' && context.aliasOrTable) {
        // エイリアス.カラム補完
        const aliasInfo = aliases.find((a) => a.alias === context.aliasOrTable?.toLowerCase());
        const tableName = aliasInfo?.tableName ?? context.aliasOrTable;

        if (connectionId) {
          const columns = await useSchemaStore.getState().loadColumns(connectionId, tableName);
          for (const col of columns) {
            suggestions.push({
              label: col.name,
              kind: 5, // Field
              detail: `${col.type}${col.nullable ? ' (nullable)' : ''}`,
              insertText: col.name,
              range,
            });
          }
        }
      } else if (context.type === 'table') {
        // テーブル名補完
        if (connectionId) {
          await useSchemaStore.getState().loadTables(connectionId);
          const tables = useSchemaStore.getState().getTables(connectionId);
          for (const table of tables) {
            const displayName =
              table.schema !== 'dbo' ? `${table.schema}.${table.name}` : table.name;
            suggestions.push({
              label: displayName,
              kind: table.type === 'VIEW' ? 1 : 6, // Class for VIEW, Struct for TABLE
              detail: table.type,
              insertText: displayName.includes('.')
                ? `[${table.schema}].[${table.name}]`
                : table.name,
              range,
            });
          }
        }
      } else if (context.type === 'column') {
        // カラム名補完（全テーブルから）
        if (connectionId) {
          // 解析済みテーブルのカラムを補完
          for (const alias of aliases) {
            const columns = useSchemaStore
              .getState()
              .getTableColumns(connectionId, alias.tableName);
            if (columns) {
              for (const col of columns) {
                suggestions.push({
                  label: `${alias.alias}.${col.name}`,
                  kind: 5, // Field
                  detail: `${alias.tableName}.${col.name} (${col.type})`,
                  insertText: `${alias.alias}.${col.name}`,
                  range,
                });
              }
            }
          }
        }
      }

      // キーワード補完は常に追加（優先度低め）
      for (const keyword of SQL_KEYWORDS) {
        if (keyword.toLowerCase().startsWith(word.word.toLowerCase())) {
          suggestions.push({
            label: keyword,
            kind: 14, // Keyword
            insertText: keyword,
            range,
            sortText: `z${keyword}`, // キーワードは後に
          });
        }
      }

      return { suggestions };
    },
  };
}
