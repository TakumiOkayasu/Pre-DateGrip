import { memo } from 'react';
import styles from './ShapeNode.module.css';

interface ShapeNodeData {
  shapeType: string;
  text: string;
  fillColor?: string;
  fontColor?: string;
  fillAlpha: number;
  fontSize: number;
  width: number;
  height: number;
}

interface ShapeNodeProps {
  data: ShapeNodeData;
}

function hexToRgba(hex: string, alpha: number): string {
  if (hex.length < 7 || hex[0] !== '#') return 'transparent';
  const r = Number.parseInt(hex.slice(1, 3), 16);
  const g = Number.parseInt(hex.slice(3, 5), 16);
  const b = Number.parseInt(hex.slice(5, 7), 16);
  if (Number.isNaN(r) || Number.isNaN(g) || Number.isNaN(b)) return 'transparent';
  const a = Math.max(0, Math.min(1, alpha / 255));
  return `rgba(${r}, ${g}, ${b}, ${a})`;
}

export const ShapeNode = memo(function ShapeNode({ data }: ShapeNodeProps) {
  const { shapeType, text, fillColor, fontColor, fillAlpha, fontSize, width, height } = data;
  const isRound = shapeType === 'roundrect';

  const backgroundColor = fillColor ? hexToRgba(fillColor, fillAlpha) : 'transparent';

  return (
    <div
      className={styles.container}
      style={{
        width: width || undefined,
        height: height || undefined,
        backgroundColor,
        borderRadius: isRound ? 8 : 0,
        color: fontColor || 'var(--text-primary)',
        fontSize: fontSize || 9,
      }}
    >
      {text && <span className={styles.text}>{text}</span>}
    </div>
  );
});
