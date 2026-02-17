import { useEffect, useRef } from 'react';

/**
 * Registers a global keydown handler that always reflects the latest callback.
 * The listener is registered once on mount and never re-registered.
 * Avoids stale closures by keeping the handler in a ref.
 *
 * @param handler - The keydown event handler (can safely close over component state)
 * @param containerRef - Optional: only fires when focus is inside the container
 */
export function useKeyboardHandler(
  handler: (e: KeyboardEvent) => void,
  containerRef?: React.RefObject<HTMLElement | null>
): void {
  const handlerRef = useRef(handler);
  // Intentionally no deps: sync ref on every render to always reflect the latest handler
  useEffect(() => {
    handlerRef.current = handler;
  });

  useEffect(() => {
    const onKeyDown = (e: KeyboardEvent) => {
      if (containerRef && !containerRef.current?.contains(document.activeElement)) return;
      handlerRef.current(e);
    };
    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [containerRef]);
}
