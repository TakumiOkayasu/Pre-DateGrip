import { useCallback, useEffect, useRef, useState } from 'react';
import { useConnectionStore } from '../../store/connectionStore';
import { useQueries, useQueryActions, useQueryStore } from '../../store/queryStore';
import type { Query } from '../../types';
import styles from './EditorTabs.module.css';

// SQL file icon
const SqlIcon = (
  <svg
    className={styles.tabIcon}
    viewBox="0 0 16 16"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.2"
    aria-hidden="true"
  >
    <path d="M9 1H4a1 1 0 00-1 1v12a1 1 0 001 1h8a1 1 0 001-1V5L9 1z" />
    <path d="M9 1v4h4" />
  </svg>
);

// ER diagram icon
const ERDiagramIcon = (
  <svg
    className={styles.tabIcon}
    viewBox="0 0 16 16"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.2"
    aria-hidden="true"
  >
    <rect x="1" y="1" width="5" height="4" rx="0.5" />
    <rect x="10" y="1" width="5" height="4" rx="0.5" />
    <rect x="5.5" y="11" width="5" height="4" rx="0.5" />
    <path d="M3.5 5v3.5h4.5V11" />
    <path d="M12.5 5v3.5h-4.5V11" />
  </svg>
);

// Plus icon for add button
const PlusIcon = (
  <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" strokeWidth="1.5" aria-hidden="true">
    <path d="M8 3v10M3 8h10" />
  </svg>
);

// Close icon
const CloseIcon = (
  <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" strokeWidth="1.5" aria-hidden="true">
    <path d="M4 4l8 8M12 4l-8 8" />
  </svg>
);

function buildTooltip(query: Query): string {
  const parts = [query.name];
  if (query.logicalName) {
    parts.push(query.logicalName);
  } else if (query.isDataView && query.sourceTable && query.sourceTable !== query.name) {
    parts.push(query.sourceTable);
  }
  if (query.connectionId) {
    const conn = useConnectionStore.getState().connections.find((c) => c.id === query.connectionId);
    if (conn) {
      parts.push(`接続先: ${conn.server}/${conn.database}`);
    }
  }
  return parts.join('\n');
}

export function EditorTabs() {
  const queries = useQueries();
  const activeQueryId = useQueryStore((state) => state.activeQueryId);
  const { addQuery, removeQuery, setActive } = useQueryActions();
  const activeConnectionId = useConnectionStore((s) => s.activeConnectionId);

  const tabsRef = useRef<HTMLDivElement>(null);
  const [isOverflowing, setIsOverflowing] = useState(false);
  const [menuOpen, setMenuOpen] = useState(false);
  const menuRef = useRef<HTMLDivElement>(null);

  const checkOverflow = useCallback(() => {
    const el = tabsRef.current;
    if (el) {
      setIsOverflowing(el.scrollWidth > el.clientWidth);
    }
  }, []);

  // ResizeObserver + scroll で溢れ検知
  useEffect(() => {
    const el = tabsRef.current;
    if (!el) return;

    checkOverflow();

    const observer = new ResizeObserver(checkOverflow);
    observer.observe(el);
    el.addEventListener('scroll', checkOverflow);

    return () => {
      observer.disconnect();
      el.removeEventListener('scroll', checkOverflow);
    };
  }, [checkOverflow]);

  // タブ数変更時に溢れ再チェック
  // biome-ignore lint/correctness/useExhaustiveDependencies: queries.length is intentional to recheck overflow on tab count change
  useEffect(() => {
    checkOverflow();
  }, [queries.length, checkOverflow]);

  // メニュー外クリックで閉じる
  useEffect(() => {
    if (!menuOpen) return;

    const handleClick = (e: MouseEvent) => {
      if (menuRef.current && !menuRef.current.contains(e.target as Node)) {
        setMenuOpen(false);
      }
    };
    document.addEventListener('mousedown', handleClick);
    return () => document.removeEventListener('mousedown', handleClick);
  }, [menuOpen]);

  return (
    <div className={styles.container}>
      <div className={styles.tabs} ref={tabsRef}>
        {queries.map((query) => (
          <div
            key={query.id}
            className={`${styles.tab} ${query.id === activeQueryId ? styles.active : ''}`}
            onClick={() => setActive(query.id)}
            title={buildTooltip(query)}
          >
            {query.isERDiagram ? ERDiagramIcon : SqlIcon}
            <span className={styles.tabName}>
              {query.isDirty && <span className={styles.dirty}>●</span>}
              {query.name}
            </span>
            <button
              className={styles.closeButton}
              onClick={(e) => {
                e.stopPropagation();
                removeQuery(query.id);
              }}
              title="タブを閉じる"
            >
              {CloseIcon}
            </button>
          </div>
        ))}
      </div>
      {isOverflowing && (
        <div className={styles.overflowMenuWrapper} ref={menuRef}>
          <button
            className={styles.overflowButton}
            onClick={() => setMenuOpen((prev) => !prev)}
            title="全タブ一覧"
          >
            ▾
          </button>
          {menuOpen && (
            <div className={styles.overflowMenu}>
              {queries.map((query) => (
                <button
                  key={query.id}
                  className={`${styles.overflowMenuItem} ${query.id === activeQueryId ? styles.activeItem : ''}`}
                  onClick={() => {
                    setActive(query.id);
                    setMenuOpen(false);
                  }}
                >
                  {query.isERDiagram ? ERDiagramIcon : SqlIcon}
                  <span className={styles.overflowMenuItemName}>{query.name}</span>
                </button>
              ))}
            </div>
          )}
        </div>
      )}
      <button
        className={styles.addButton}
        onClick={() => addQuery(activeConnectionId)}
        title="新規クエリ (Ctrl+N)"
      >
        {PlusIcon}
      </button>
    </div>
  );
}
