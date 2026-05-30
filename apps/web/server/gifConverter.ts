import { parseGIF, decompressFrames, type ParsedFrame } from "gifuct-js";

const kPetMaxSize = 118;
const kPkaHeaderSize = 12;
const kPkaFrameEntrySize = 10;

export interface ConvertedGif {
  sourceWidth: number;
  sourceHeight: number;
  deviceWidth: number;
  deviceHeight: number;
  frameCount: number;
  durationMs: number;
  fpsEstimate: number;
  frameDelaysMs: number[];
  packageBytes: Buffer;
}

export function convertGifToPka(bytes: Buffer, maxSize = kPetMaxSize): ConvertedGif {
  const arrayBuffer = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength) as ArrayBuffer;
  const parsed = parseGIF(arrayBuffer);
  const frames = decompressFrames(parsed, true);
  if (frames.length === 0) {
    throw new Error("GIF 没有可用帧");
  }

  const sourceWidth = parsed.lsd.width;
  const sourceHeight = parsed.lsd.height;
  if (sourceWidth <= 0 || sourceHeight <= 0) {
    throw new Error("GIF 尺寸无效");
  }

  const scale = Math.min(1, maxSize / Math.max(sourceWidth, sourceHeight));
  const deviceWidth = Math.max(1, Math.round(sourceWidth * scale));
  const deviceHeight = Math.max(1, Math.round(sourceHeight * scale));
  const frameDelaysMs = frames.map((frame) => Math.max(20, Math.round(frame.delay || 100)));
  const durationMs = frameDelaysMs.reduce((sum, delay) => sum + delay, 0);
  const fpsEstimate = Math.max(1, Math.min(60, Math.round((frames.length * 1000) / Math.max(durationMs, 1))));

  const fullFrames = compositeFrames(frames, sourceWidth, sourceHeight).map((rgba) =>
    encodeRgb565Rle(scaleRgbaNearest(rgba, sourceWidth, sourceHeight, deviceWidth, deviceHeight))
  );
  const packageBytes = buildPka(deviceWidth, deviceHeight, fpsEstimate, frameDelaysMs, fullFrames);

  return {
    sourceWidth,
    sourceHeight,
    deviceWidth,
    deviceHeight,
    frameCount: frames.length,
    durationMs,
    fpsEstimate,
    frameDelaysMs,
    packageBytes
  };
}

function compositeFrames(frames: ParsedFrame[], width: number, height: number) {
  const canvas = new Uint8ClampedArray(width * height * 4);
  const output: Uint8ClampedArray[] = [];

  for (const frame of frames) {
    const previous = frame.disposalType === 3 ? new Uint8ClampedArray(canvas) : null;
    drawPatch(canvas, width, height, frame);
    output.push(new Uint8ClampedArray(canvas));

    if (frame.disposalType === 2) {
      clearPatch(canvas, width, height, frame);
    } else if (frame.disposalType === 3 && previous) {
      canvas.set(previous);
    }
  }

  return output;
}

function drawPatch(canvas: Uint8ClampedArray, width: number, height: number, frame: ParsedFrame) {
  const { left, top, width: patchWidth, height: patchHeight } = frame.dims;
  for (let y = 0; y < patchHeight; y += 1) {
    const targetY = top + y;
    if (targetY < 0 || targetY >= height) {
      continue;
    }
    for (let x = 0; x < patchWidth; x += 1) {
      const targetX = left + x;
      if (targetX < 0 || targetX >= width) {
        continue;
      }

      const sourceIndex = (y * patchWidth + x) * 4;
      const alpha = frame.patch[sourceIndex + 3];
      if (alpha === 0) {
        continue;
      }

      const targetIndex = (targetY * width + targetX) * 4;
      canvas[targetIndex] = frame.patch[sourceIndex];
      canvas[targetIndex + 1] = frame.patch[sourceIndex + 1];
      canvas[targetIndex + 2] = frame.patch[sourceIndex + 2];
      canvas[targetIndex + 3] = 255;
    }
  }
}

function clearPatch(canvas: Uint8ClampedArray, width: number, height: number, frame: ParsedFrame) {
  const { left, top, width: patchWidth, height: patchHeight } = frame.dims;
  for (let y = 0; y < patchHeight; y += 1) {
    const targetY = top + y;
    if (targetY < 0 || targetY >= height) {
      continue;
    }
    for (let x = 0; x < patchWidth; x += 1) {
      const targetX = left + x;
      if (targetX < 0 || targetX >= width) {
        continue;
      }
      const targetIndex = (targetY * width + targetX) * 4;
      canvas[targetIndex] = 0;
      canvas[targetIndex + 1] = 0;
      canvas[targetIndex + 2] = 0;
      canvas[targetIndex + 3] = 0;
    }
  }
}

function scaleRgbaNearest(
  rgba: Uint8ClampedArray,
  sourceWidth: number,
  sourceHeight: number,
  targetWidth: number,
  targetHeight: number
) {
  const pixels = new Uint16Array(targetWidth * targetHeight);
  for (let y = 0; y < targetHeight; y += 1) {
    const sourceY = Math.min(sourceHeight - 1, Math.floor((y * sourceHeight) / targetHeight));
    for (let x = 0; x < targetWidth; x += 1) {
      const sourceX = Math.min(sourceWidth - 1, Math.floor((x * sourceWidth) / targetWidth));
      const sourceIndex = (sourceY * sourceWidth + sourceX) * 4;
      const alpha = rgba[sourceIndex + 3];
      pixels[y * targetWidth + x] =
        alpha === 0 ? 0 : rgbTo565(rgba[sourceIndex], rgba[sourceIndex + 1], rgba[sourceIndex + 2]);
    }
  }
  return pixels;
}

function rgbTo565(red: number, green: number, blue: number) {
  return ((red & 0xf8) << 8) | ((green & 0xfc) << 3) | (blue >> 3);
}

function encodeRgb565Rle(pixels: Uint16Array) {
  const chunks: Buffer[] = [];
  let index = 0;
  while (index < pixels.length) {
    const color = pixels[index];
    let runLength = 1;
    while (index + runLength < pixels.length && pixels[index + runLength] === color && runLength < 0xffff) {
      runLength += 1;
    }

    const chunk = Buffer.alloc(4);
    chunk.writeUInt16LE(runLength, 0);
    chunk.writeUInt16LE(color, 2);
    chunks.push(chunk);
    index += runLength;
  }

  return Buffer.concat(chunks);
}

function buildPka(
  width: number,
  height: number,
  fpsEstimate: number,
  frameDelaysMs: number[],
  frames: Buffer[]
) {
  const tableSize = frames.length * kPkaFrameEntrySize;
  let offset = kPkaHeaderSize + tableSize;
  const totalSize = offset + frames.reduce((sum, frame) => sum + frame.length, 0);
  const output = Buffer.alloc(totalSize);

  output.write("PKA1", 0, "ascii");
  output.writeUInt16LE(width, 4);
  output.writeUInt16LE(height, 6);
  output.writeUInt16LE(frames.length, 8);
  output.writeUInt16LE(fpsEstimate, 10);

  frames.forEach((frame, index) => {
    const entryOffset = kPkaHeaderSize + index * kPkaFrameEntrySize;
    output.writeUInt32LE(offset, entryOffset);
    output.writeUInt32LE(frame.length, entryOffset + 4);
    output.writeUInt16LE(frameDelaysMs[index] ?? 100, entryOffset + 8);
    frame.copy(output, offset);
    offset += frame.length;
  });

  return output;
}
