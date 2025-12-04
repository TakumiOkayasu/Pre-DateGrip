import { useActiveQuery } from '../../store/queryStore';
import { EditorTabs } from '../editor/EditorTabs';
import { SqlEditor } from '../editor/SqlEditor';
import { ResultGrid } from '../grid/ResultGrid';
import styles from './CenterPanel.module.css';

export function CenterPanel() {
  const activeQuery = useActiveQuery();
  const isDataView = activeQuery?.isDataView === true;

  return (
    <div className={styles.container}>
      <EditorTabs />
      <div className={styles.editorContainer}>
        {isDataView ? <ResultGrid queryId={activeQuery?.id} /> : <SqlEditor />}
      </div>
    </div>
  );
}
