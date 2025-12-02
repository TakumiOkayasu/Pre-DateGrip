import { useState, useEffect, useRef, useCallback } from 'react'
import type { ICellEditorParams } from 'ag-grid-community'
import styles from './CellEditor.module.css'

interface CellEditorProps extends ICellEditorParams {
  onValueChange?: (value: string | null) => void
}

export function CellEditor(props: CellEditorProps) {
  const { value, onValueChange } = props
  const [currentValue, setCurrentValue] = useState<string>(value ?? '')
  const [isNull, setIsNull] = useState<boolean>(value === null)
  const inputRef = useRef<HTMLTextAreaElement>(null)

  useEffect(() => {
    if (inputRef.current) {
      inputRef.current.focus()
      inputRef.current.select()
    }
  }, [])

  const handleChange = useCallback((e: React.ChangeEvent<HTMLTextAreaElement>) => {
    setCurrentValue(e.target.value)
    setIsNull(false)
    onValueChange?.(e.target.value)
  }, [onValueChange])

  const handleSetNull = useCallback(() => {
    setIsNull(true)
    setCurrentValue('')
    onValueChange?.(null)
  }, [onValueChange])

  const handleClearNull = useCallback(() => {
    setIsNull(false)
    inputRef.current?.focus()
  }, [])

  const getValue = useCallback(() => {
    return isNull ? null : currentValue
  }, [isNull, currentValue])

  const isCancelBeforeStart = useCallback(() => {
    return false
  }, [])

  const isCancelAfterEnd = useCallback(() => {
    return false
  }, [])

  // Expose methods for AG Grid
  useEffect(() => {
    const api = {
      getValue,
      isCancelBeforeStart,
      isCancelAfterEnd,
    }
    Object.assign(props, api)
  }, [props, getValue, isCancelBeforeStart, isCancelAfterEnd])

  return (
    <div className={styles.container}>
      <div className={styles.inputWrapper}>
        <textarea
          ref={inputRef}
          value={isNull ? '' : currentValue}
          onChange={handleChange}
          className={`${styles.input} ${isNull ? styles.nullInput : ''}`}
          disabled={isNull}
          placeholder={isNull ? 'NULL' : ''}
          rows={1}
        />
      </div>
      <div className={styles.actions}>
        {isNull ? (
          <button
            type="button"
            className={styles.actionButton}
            onClick={handleClearNull}
            title="Clear NULL"
          >
            Clear NULL
          </button>
        ) : (
          <button
            type="button"
            className={`${styles.actionButton} ${styles.nullButton}`}
            onClick={handleSetNull}
            title="Set to NULL"
          >
            Set NULL
          </button>
        )}
      </div>
    </div>
  )
}

// AG Grid cell editor wrapper
export const CellEditorComponent = {
  create: (params: ICellEditorParams) => {
    const container = document.createElement('div')
    container.className = 'cell-editor-container'

    let currentValue = params.value
    let isNull = params.value === null

    const input = document.createElement('textarea')
    input.className = 'cell-editor-input'
    input.value = isNull ? '' : (currentValue ?? '')
    input.placeholder = isNull ? 'NULL' : ''
    input.rows = 1
    if (isNull) {
      input.disabled = true
      input.classList.add('null-input')
    }

    const actionsDiv = document.createElement('div')
    actionsDiv.className = 'cell-editor-actions'

    const nullButton = document.createElement('button')
    nullButton.type = 'button'
    nullButton.className = 'cell-editor-action-button'
    nullButton.textContent = isNull ? 'Clear NULL' : 'Set NULL'
    nullButton.onclick = () => {
      if (isNull) {
        isNull = false
        input.disabled = false
        input.classList.remove('null-input')
        input.placeholder = ''
        nullButton.textContent = 'Set NULL'
        input.focus()
      } else {
        isNull = true
        currentValue = null
        input.value = ''
        input.disabled = true
        input.classList.add('null-input')
        input.placeholder = 'NULL'
        nullButton.textContent = 'Clear NULL'
      }
    }

    input.oninput = () => {
      currentValue = input.value
      isNull = false
    }

    actionsDiv.appendChild(nullButton)
    container.appendChild(input)
    container.appendChild(actionsDiv)

    return {
      getGui: () => container,
      getValue: () => isNull ? null : currentValue,
      afterGuiAttached: () => {
        input.focus()
        input.select()
      },
      isCancelBeforeStart: () => false,
      isCancelAfterEnd: () => false,
    }
  },
}
