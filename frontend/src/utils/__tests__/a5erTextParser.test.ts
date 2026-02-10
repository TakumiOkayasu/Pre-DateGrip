import { describe, expect, it } from 'vitest';
import { A5ERTextParser } from '../a5erTextParser';

const parser = new A5ERTextParser();

// === canParse() ===

describe('A5ERTextParser.canParse', () => {
  it('先頭が # A5:ER で始まるファイルを認識する', () => {
    expect(parser.canParse('# A5:ER FORMAT:19\n# A5:ER ENCODING:UTF-8\n')).toBe(true);
  });

  it('[Entity] セクションがあるファイルを認識する', () => {
    expect(parser.canParse('[Entity]\nPName=users\nDEL\n')).toBe(true);
  });

  it('無関係なテキストは false', () => {
    expect(parser.canParse('SELECT * FROM users')).toBe(false);
  });

  it('XML形式のA5ERは false', () => {
    expect(parser.canParse('<?xml version="1.0"?><A5ER>')).toBe(false);
  });
});

// === 基本パース ===

const BASIC_INPUT = `# A5:ER FORMAT:19
# A5:ER ENCODING:UTF-8

[Entity]
PName=users
LName=ユーザー
Comment=ユーザーマスター
Page=MAIN
Left=100
Top=200
Field="id","id","INT","NOT NULL",1,"",""
Field="name","名前","NVARCHAR(100)","NOT NULL",0,"",""
Field="email","メール","NVARCHAR(255)","NULL",0,"",""
DEL

[Entity]
PName=orders
LName=注文
Comment=
Page=MAIN
Left=400
Top=200
Field="id","id","INT","NOT NULL",1,"",""
Field="user_id","ユーザーID","INT","NOT NULL",0,"",""
Field="amount","金額","DECIMAL(10,2)","NOT NULL",0,"0",""
DEL

[Relation]
Entity1=users
Entity2=orders
RelationType1=2
RelationType2=3
Fields1=id
Fields2=user_id
Dependence=0
Position="MAIN",0,5013,5187,5100,R,R
DEL
`;

describe('A5ERTextParser.parse - 基本', () => {
  const model = parser.parse(BASIC_INPUT, 'test.a5er');

  it('ファイル名がモデル名になる', () => {
    expect(model.name).toBe('test.a5er');
  });

  it('2テーブルを解析する', () => {
    expect(model.tables).toHaveLength(2);
  });

  it('テーブル名・論理名・コメントを取得する', () => {
    const users = model.tables[0];
    expect(users.name).toBe('users');
    expect(users.logicalName).toBe('ユーザー');
    expect(users.comment).toBe('ユーザーマスター');
  });

  it('テーブルの位置を取得する', () => {
    const users = model.tables[0];
    expect(users.posX).toBe(100);
    expect(users.posY).toBe(200);
  });

  it('カラムを正しく解析する', () => {
    const users = model.tables[0];
    expect(users.columns).toHaveLength(3);

    const idCol = users.columns[0];
    expect(idCol.name).toBe('id');
    expect(idCol.logicalName).toBe('id');
    expect(idCol.type).toBe('INT');
    expect(idCol.nullable).toBe(false);
    expect(idCol.isPrimaryKey).toBe(true);

    const nameCol = users.columns[1];
    expect(nameCol.name).toBe('name');
    expect(nameCol.logicalName).toBe('名前');
    expect(nameCol.type).toBe('NVARCHAR(100)');
    expect(nameCol.nullable).toBe(false);
    expect(nameCol.isPrimaryKey).toBe(false);

    const emailCol = users.columns[2];
    expect(emailCol.nullable).toBe(true);
  });

  it('デフォルト値を取得する', () => {
    const orders = model.tables[1];
    const amountCol = orders.columns[2];
    expect(amountCol.defaultValue).toBe('0');
  });

  it('1リレーションを解析する', () => {
    expect(model.relations).toHaveLength(1);
    const rel = model.relations[0];
    expect(rel.sourceTable).toBe('users');
    expect(rel.targetTable).toBe('orders');
    expect(rel.sourceColumn).toBe('id');
    expect(rel.targetColumn).toBe('user_id');
    expect(rel.cardinality).toBe('1:N');
  });
});

// === RelationType → cardinality 変換 ===

describe('RelationType変換', () => {
  const makeRelation = (type1: number, type2: number) => `# A5:ER FORMAT:19

[Entity]
PName=a
LName=A
Field="id","id","INT","NOT NULL",1,"",""
DEL

[Entity]
PName=b
LName=B
Field="id","id","INT","NOT NULL",1,"",""
DEL

[Relation]
Entity1=a
Entity2=b
RelationType1=${type1}
RelationType2=${type2}
Fields1=id
Fields2=id
DEL
`;

  it('(2,3) → 1:N', () => {
    const model = parser.parse(makeRelation(2, 3));
    expect(model.relations[0].cardinality).toBe('1:N');
  });

  it('(2,4) → 1:N', () => {
    const model = parser.parse(makeRelation(2, 4));
    expect(model.relations[0].cardinality).toBe('1:N');
  });

  it('(2,2) → 1:1', () => {
    const model = parser.parse(makeRelation(2, 2));
    expect(model.relations[0].cardinality).toBe('1:1');
  });

  it('(2,1) → 1:1', () => {
    const model = parser.parse(makeRelation(2, 1));
    expect(model.relations[0].cardinality).toBe('1:1');
  });

  it('(1,2) → 1:1', () => {
    const model = parser.parse(makeRelation(1, 2));
    expect(model.relations[0].cardinality).toBe('1:1');
  });

  it('(3,3) → N:M', () => {
    const model = parser.parse(makeRelation(3, 3));
    expect(model.relations[0].cardinality).toBe('N:M');
  });

  it('(4,4) → N:M', () => {
    const model = parser.parse(makeRelation(4, 4));
    expect(model.relations[0].cardinality).toBe('N:M');
  });

  it('(3,4) → N:M', () => {
    const model = parser.parse(makeRelation(3, 4));
    expect(model.relations[0].cardinality).toBe('N:M');
  });
});

// === Index解析 ===

describe('Index解析', () => {
  const INPUT_WITH_INDEX = `# A5:ER FORMAT:19

[Entity]
PName=users
LName=ユーザー
Field="id","id","INT","NOT NULL",1,"",""
Field="email","メール","NVARCHAR(255)","NOT NULL",0,"",""
Field="name","名前","NVARCHAR(100)","NULL",0,"",""
Index=IX_users_email=0,email
Index=UQ_users_name=1,name
DEL
`;

  it('インデックスを解析する', () => {
    const model = parser.parse(INPUT_WITH_INDEX);
    const table = model.tables[0];
    expect(table.indexes).toHaveLength(2);

    expect(table.indexes[0].name).toBe('IX_users_email');
    expect(table.indexes[0].isUnique).toBe(false);
    expect(table.indexes[0].columns).toEqual(['email']);

    expect(table.indexes[1].name).toBe('UQ_users_name');
    expect(table.indexes[1].isUnique).toBe(true);
    expect(table.indexes[1].columns).toEqual(['name']);
  });
});

// === エッジケース ===

describe('エッジケース', () => {
  it('空のEntityを処理する', () => {
    const input = `# A5:ER FORMAT:19

[Entity]
PName=empty_table
LName=空テーブル
DEL
`;
    const model = parser.parse(input);
    expect(model.tables).toHaveLength(1);
    expect(model.tables[0].columns).toHaveLength(0);
  });

  it('Field内のカンマを含む型名を正しく処理する', () => {
    const input = `# A5:ER FORMAT:19

[Entity]
PName=test
LName=テスト
Field="price","価格","DECIMAL(10,2)","NOT NULL",0,"",""
DEL
`;
    const model = parser.parse(input);
    expect(model.tables[0].columns[0].type).toBe('DECIMAL(10,2)');
  });

  it('空文字列のコメント・デフォルト値', () => {
    const input = `# A5:ER FORMAT:19

[Entity]
PName=test
LName=テスト
Field="id","id","INT","NOT NULL",1,"",""
DEL
`;
    const model = parser.parse(input);
    const col = model.tables[0].columns[0];
    expect(col.defaultValue).toBe('');
    expect(col.comment).toBe('');
  });

  it('名前なしのモデルは空文字列', () => {
    const input = `# A5:ER FORMAT:19

[Entity]
PName=test
LName=テスト
Field="id","id","INT","NOT NULL",1,"",""
DEL
`;
    const model = parser.parse(input);
    expect(model.name).toBe('');
  });

  it('Entityが0個でもエラーにならない', () => {
    const input = '# A5:ER FORMAT:19\n';
    const model = parser.parse(input);
    expect(model.tables).toHaveLength(0);
    expect(model.relations).toHaveLength(0);
  });
});
