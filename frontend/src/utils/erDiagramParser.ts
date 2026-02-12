// === 共通中間表現 ===

export interface ERDiagramModel {
  name: string;
  tables: ERDiagramTable[];
  relations: ERDiagramRelation[];
}

export interface ERDiagramTable {
  name: string;
  logicalName: string;
  comment: string;
  page: string;
  posX: number;
  posY: number;
  columns: ERDiagramColumn[];
  indexes: ERDiagramIndex[];
}

export interface ERDiagramColumn {
  name: string;
  logicalName: string;
  type: string;
  nullable: boolean;
  isPrimaryKey: boolean;
  defaultValue: string;
  comment: string;
}

export interface ERDiagramIndex {
  name: string;
  isUnique: boolean;
  columns: string[];
}

export interface ERDiagramRelation {
  name: string;
  sourceTable: string;
  targetTable: string;
  sourceColumn: string;
  targetColumn: string;
  cardinality: '1:1' | '1:N' | 'N:M';
}

// === パーサーインターフェース ===

export interface ERDiagramParser {
  /** このパーサーが対応するファイル拡張子 */
  readonly extensions: string[];
  /** ファイル内容がこのパーサーで処理可能か判定 */
  canParse(content: string): boolean;
  /** パース実行 */
  parse(content: string, name?: string): ERDiagramModel;
}

// === パーサーレジストリ ===

const parsers: ERDiagramParser[] = [];

export function registerERDiagramParser(parser: ERDiagramParser): void {
  parsers.push(parser);
}

export function parseERDiagram(content: string, filename?: string): ERDiagramModel {
  // 1. filenameの拡張子でパーサー候補を絞る
  if (filename) {
    const rawExt = filename.split('.').pop()?.toLowerCase();
    if (rawExt) {
      const ext = `.${rawExt}`;
      for (const parser of parsers) {
        if (parser.extensions.includes(ext) && parser.canParse(content)) {
          return parser.parse(content, filename);
        }
      }
    }
  }

  // 2. 全パーサーでcanParse()判定
  for (const parser of parsers) {
    if (parser.canParse(content)) {
      return parser.parse(content, filename);
    }
  }

  throw new Error('対応するER図パーサーが見つかりません');
}
