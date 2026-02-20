import { memo } from 'react';
import { blendColor, hexToRgba, readableColor } from '../../utils/colorContrast';
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

const CANVAS_BG = '#1e1e1e';

export const ShapeNode = memo(function ShapeNode({ data }: ShapeNodeProps) {
  const { shapeType, text, fillColor, fontColor, fillAlpha, fontSize, width, height } = data;
  const isRound = shapeType === 'roundrect';

  const backgroundColor = fillColor ? hexToRgba(fillColor, fillAlpha) : 'transparent';

  const effectiveBg = fillColor ? blendColor(fillColor, fillAlpha, CANVAS_BG) : CANVAS_BG;
  const textColor = fontColor ? readableColor(effectiveBg, fontColor) : undefined;

  return (
    <div
      className={styles.container}
      style={{
        width: width || undefined,
        height: height || undefined,
        backgroundColor,
        borderRadius: isRound ? 8 : 0,
        color: textColor || 'var(--text-primary)',
        fontSize: fontSize || 9,
      }}
    >
      {text && <span className={styles.text}>{text}</span>}
    </div>
  );
});
