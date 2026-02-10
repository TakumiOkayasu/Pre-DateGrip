import { lazy, Suspense } from 'react';
import { useERDiagramStore } from '../../store/erDiagramStore';
import { useActiveQuery } from '../../store/queryStore';
import { log } from '../../utils/logger';
import { EditorTabs } from '../editor/EditorTabs';
import styles from './CenterPanel.module.css';

// Lazy load heavy components to reduce initial bundle size
const SqlEditor = lazy(() =>
  import('../editor/SqlEditor').then((module) => ({ default: module.SqlEditor }))
);
const ResultGrid = lazy(() =>
  import('../grid/ResultGrid').then((module) => ({ default: module.ResultGrid }))
);
const ERDiagramView = lazy(() =>
  import('../diagram/ERDiagram').then((module) => ({ default: module.ERDiagram }))
);

// Loading fallback for Monaco Editor
function EditorLoadingFallback() {
  return (
    <div className={styles.editorLoading}>
      <div className={styles.loadingSpinner} />
      <div className={styles.loadingText}>Loading editor...</div>
    </div>
  );
}

export function CenterPanel() {
  const activeQuery = useActiveQuery();
  const isDataView = activeQuery?.isDataView === true;
  const isERDiagram = activeQuery?.isERDiagram === true;
  const tables = useERDiagramStore((s) => s.tables);
  const relations = useERDiagramStore((s) => s.relations);

  log.debug(
    `[CenterPanel] Render: activeQuery=${activeQuery?.id}, isDataView=${isDataView}, isERDiagram=${isERDiagram}`
  );

  return (
    <div className={styles.container}>
      <EditorTabs />
      <div className={styles.editorContainer}>
        <Suspense fallback={<EditorLoadingFallback />}>
          {isERDiagram ? (
            <ERDiagramView tables={tables} relations={relations} />
          ) : isDataView ? (
            <ResultGrid queryId={activeQuery?.id} />
          ) : (
            <SqlEditor />
          )}
        </Suspense>
      </div>
    </div>
  );
}
