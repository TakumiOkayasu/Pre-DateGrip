import type {
  ERDiagramColumn,
  ERDiagramIndex,
  ERDiagramModel,
  ERDiagramParser,
  ERDiagramRelation,
  ERDiagramTable,
} from './erDiagramParser';
import { registerERDiagramParser } from './erDiagramParser';

/**
 * A5:SQL Mk-2 テキスト形式 (.a5er) パーサー
 *
 * フォーマット仕様:
 * - セクション: [Entity], [Relation] 等
 * - セクション末尾: DEL
 * - Field="物理名","論理名","型","NOT NULL|NULL",PK順序,"デフォルト","コメント"
 * - RelationType: 0=NoDraw, 1=ZeroOne, 2=One, 3=ZeroMany, 4=OneMany, 5=FixedN
 */
export class A5ERTextParser implements ERDiagramParser {
  readonly extensions = ['.a5er'];

  canParse(content: string): boolean {
    // XML形式は除外
    if (content.trimStart().startsWith('<?xml') || content.trimStart().startsWith('<')) {
      return false;
    }
    return content.startsWith('# A5:ER') || content.includes('[Entity]');
  }

  parse(content: string, name?: string): ERDiagramModel {
    const tables: ERDiagramTable[] = [];
    const relations: ERDiagramRelation[] = [];

    const sections = splitSections(content);

    for (const section of sections) {
      if (section.type === 'Entity') {
        tables.push(parseEntity(section.lines));
      } else if (section.type === 'Relation') {
        relations.push(parseRelation(section.lines));
      }
    }

    return { name: name ?? '', tables, relations };
  }
}

// === セクション分割 ===

interface Section {
  type: string;
  lines: string[];
}

function splitSections(content: string): Section[] {
  const sections: Section[] = [];
  const allLines = content.split(/\r?\n/);

  let currentType: string | null = null;
  let currentLines: string[] = [];

  for (const line of allLines) {
    const sectionMatch = line.match(/^\[(\w+)\]$/);
    if (sectionMatch) {
      currentType = sectionMatch[1];
      currentLines = [];
      continue;
    }

    if (line.trim() === 'DEL' && currentType) {
      sections.push({ type: currentType, lines: currentLines });
      currentType = null;
      currentLines = [];
      continue;
    }

    if (currentType) {
      currentLines.push(line);
    }
  }

  return sections;
}

// === Entity解析 ===

function parseEntity(lines: string[]): ERDiagramTable {
  const props = new Map<string, string>();
  const columns: ERDiagramColumn[] = [];
  const indexes: ERDiagramIndex[] = [];

  for (const line of lines) {
    if (line.startsWith('Field=')) {
      columns.push(parseField(line.slice(6)));
    } else if (line.startsWith('Index=')) {
      indexes.push(parseIndex(line.slice(6)));
    } else {
      const eqIdx = line.indexOf('=');
      if (eqIdx > 0) {
        props.set(line.slice(0, eqIdx), line.slice(eqIdx + 1));
      }
    }
  }

  return {
    name: props.get('PName') ?? '',
    logicalName: props.get('LName') ?? '',
    comment: props.get('Comment') ?? '',
    posX: parseInt(props.get('Left') ?? '0', 10) || 0,
    posY: parseInt(props.get('Top') ?? '0', 10) || 0,
    columns,
    indexes,
  };
}

// === Field解析 ===
// Field="物理名","論理名","型","NOT NULL|NULL",PK順序,"デフォルト","コメント"

function parseField(raw: string): ERDiagramColumn {
  const parts = parseQuotedCSV(raw);

  const physicalName = parts[0] ?? '';
  const logicalName = parts[1] ?? '';
  const type = parts[2] ?? '';
  const nullability = parts[3] ?? '';
  const pkOrder = parseInt(parts[4] ?? '0', 10);
  const defaultValue = parts[5] ?? '';
  const comment = parts[6] ?? '';

  return {
    name: physicalName,
    logicalName,
    type,
    nullable: nullability !== 'NOT NULL',
    isPrimaryKey: pkOrder > 0,
    defaultValue,
    comment,
  };
}

/**
 * 引用符付きCSVをパース（引用符内のカンマに対応）
 * 例: "id","名前","DECIMAL(10,2)","NOT NULL",1,"",""
 */
function parseQuotedCSV(raw: string): string[] {
  const result: string[] = [];
  let i = 0;
  const len = raw.length;

  while (i < len) {
    // 先頭空白スキップ
    while (i < len && raw[i] === ' ') i++;

    if (i >= len) break;

    if (raw[i] === '"') {
      // 引用符付きフィールド
      i++; // 開始引用符スキップ
      let value = '';
      while (i < len) {
        if (raw[i] === '"') {
          if (i + 1 < len && raw[i + 1] === '"') {
            // エスケープされた引用符
            value += '"';
            i += 2;
          } else {
            // 終了引用符
            i++;
            break;
          }
        } else {
          value += raw[i];
          i++;
        }
      }
      result.push(value);
    } else {
      // 非引用符フィールド
      let value = '';
      while (i < len && raw[i] !== ',') {
        value += raw[i];
        i++;
      }
      result.push(value);
    }

    // カンマスキップ
    if (i < len && raw[i] === ',') i++;
  }

  return result;
}

// === Index解析 ===
// Index=名前=unique(0|1),col1,col2,...

function parseIndex(raw: string): ERDiagramIndex {
  const eqIdx = raw.indexOf('=');
  if (eqIdx < 0) {
    return { name: raw, isUnique: false, columns: [] };
  }

  const name = raw.slice(0, eqIdx);
  const rest = raw.slice(eqIdx + 1);
  const parts = rest.split(',');

  const isUnique = parts[0] === '1';
  const columns = parts.slice(1);

  return { name, isUnique, columns };
}

// === Relation解析 ===

function parseRelation(lines: string[]): ERDiagramRelation {
  const props = new Map<string, string>();
  for (const line of lines) {
    const eqIdx = line.indexOf('=');
    if (eqIdx > 0) {
      props.set(line.slice(0, eqIdx), line.slice(eqIdx + 1));
    }
  }

  const type1 = parseInt(props.get('RelationType1') ?? '0', 10);
  const type2 = parseInt(props.get('RelationType2') ?? '0', 10);

  return {
    name: `${props.get('Entity1') ?? ''}_${props.get('Entity2') ?? ''}`,
    sourceTable: props.get('Entity1') ?? '',
    targetTable: props.get('Entity2') ?? '',
    sourceColumn: props.get('Fields1') ?? '',
    targetColumn: props.get('Fields2') ?? '',
    cardinality: resolveCardinality(type1, type2),
  };
}

/**
 * RelationType → cardinality 変換
 * 0=NoDraw, 1=ZeroOne(0..1), 2=One(1), 3=ZeroMany(0..*), 4=OneMany(1..*), 5=FixedN
 */
function resolveCardinality(type1: number, type2: number): '1:1' | '1:N' | 'N:M' {
  const isMany = (t: number) => t === 3 || t === 4;
  const isOne = (t: number) => t === 1 || t === 2;

  if (isMany(type1) && isMany(type2)) return 'N:M';
  if (isOne(type1) && isMany(type2)) return '1:N';
  if (isMany(type1) && isOne(type2)) return '1:N';
  if (isOne(type1) && isOne(type2)) return '1:1';

  // デフォルト: 1:N
  return '1:N';
}

// パーサー登録
registerERDiagramParser(new A5ERTextParser());
