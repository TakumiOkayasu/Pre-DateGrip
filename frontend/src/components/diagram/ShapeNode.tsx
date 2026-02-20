import { memo } from 'react';
import { hexToRgba, readableColor } from '../../utils/colorContrast';
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

  const effectiveBg = fillColor ?? CANVAS_BG;
  const textColor = readableColor(effectiveBg, fontColor);

  return (
    <div
      className={styles.container}
      style={{
        width: width || undefined,
        height: height || undefined,
        backgroundColor,
        borderRadius: isRound ? 8 : 0,
        color: textColor,
        fontSize: fontSize || 9,
      }}
    >
      {text && <span className={styles.text}>{text}</span>}
    </div>
  );
});
