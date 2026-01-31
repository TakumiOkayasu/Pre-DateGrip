/**
 * SQLパース関連のユーティリティ
 */

interface StatementRange {
  start: number;
  end: number;
  text: string;
}

/**
 * SQL文字列を個別のステートメントに分割
 * セミコロンで区切るが、文字列リテラル内のセミコロンは無視
 *
 * @param sql - SQL文字列
 * @returns 各ステートメントの範囲と内容
 */
export function parseStatements(sql: string): StatementRange[] {
  const statements: StatementRange[] = [];
  let currentStart = 0;
  let inSingleQuote = false;
  let inDoubleQuote = false;
  let inLineComment = false;
  let inBlockComment = false;

  for (let i = 0; i < sql.length; i++) {
    const char = sql[i];
    const nextChar = sql[i + 1];

    // 改行でラインコメント終了
    if (inLineComment && char === '\n') {
      inLineComment = false;
      continue;
    }

    // ラインコメント中はスキップ
    if (inLineComment) continue;

    // ブロックコメント終了
    if (inBlockComment && char === '*' && nextChar === '/') {
      inBlockComment = false;
      i++; // skip '/'
      continue;
    }

    // ブロックコメント中はスキップ
    if (inBlockComment) continue;

    // コメント開始判定（クォート外のみ）
    if (!inSingleQuote && !inDoubleQuote) {
      if (char === '-' && nextChar === '-') {
        inLineComment = true;
        i++; // skip second '-'
        continue;
      }
      if (char === '/' && nextChar === '*') {
        inBlockComment = true;
        i++; // skip '*'
        continue;
      }
    }

    // シングルクォートのトグル
    if (char === "'" && !inDoubleQuote) {
      // エスケープされたクォート（''）をスキップ
      if (inSingleQuote && nextChar === "'") {
        i++;
        continue;
      }
      inSingleQuote = !inSingleQuote;
      continue;
    }

    // ダブルクォートのトグル
    if (char === '"' && !inSingleQuote) {
      inDoubleQuote = !inDoubleQuote;
      continue;
    }

    // セミコロンでステートメント区切り（クォート外のみ）
    if (char === ';' && !inSingleQuote && !inDoubleQuote) {
      const text = sql.slice(currentStart, i + 1).trim();
      if (text.length > 0) {
        statements.push({
          start: currentStart,
          end: i + 1,
          text,
        });
      }
      currentStart = i + 1;
    }
  }

  // 最後のステートメント（セミコロンなし）
  const remaining = sql.slice(currentStart).trim();
  if (remaining.length > 0) {
    statements.push({
      start: currentStart,
      end: sql.length,
      text: remaining,
    });
  }

  return statements;
}

/**
 * カーソル位置のSQL文を取得
 *
 * @param sql - SQL文字列全体
 * @param cursorOffset - カーソル位置（0-indexed）
 * @returns カーソル位置のSQL文、見つからない場合はnull
 */
export function getStatementAtCursor(sql: string, cursorOffset: number): string | null {
  const statements = parseStatements(sql);

  for (const stmt of statements) {
    if (cursorOffset >= stmt.start && cursorOffset <= stmt.end) {
      return stmt.text;
    }
  }

  // カーソルがどのステートメントにも含まれない場合、最後のステートメントを返す
  if (statements.length > 0) {
    const last = statements[statements.length - 1];
    // カーソルが最後のステートメントの後にある場合
    if (cursorOffset >= last.end) {
      return last.text;
    }
  }

  return null;
}
