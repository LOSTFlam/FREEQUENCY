// Run via Figma MCP use_figma against fileKey ACH9Fh5jq0xP3BDmMQFrXr
// skillNames: figma-use,figma-generate-design

await figma.loadFontAsync({ family: 'Inter', style: 'Regular' });
await figma.loadFontAsync({ family: 'Inter', style: 'Bold' });
await figma.loadFontAsync({ family: 'Inter', style: 'Medium' });

const C = {
  void: { r: 0.039, g: 0.055, b: 0.078 },
  glass: { r: 0.071, g: 0.094, b: 0.125, a: 0.92 },
  panel: { r: 0.102, g: 0.133, b: 0.188 },
  teal: { r: 0.176, g: 0.831, b: 0.749 },
  violet: { r: 0.655, g: 0.545, b: 0.980 },
  amber: { r: 0.961, g: 0.620, b: 0.043 },
  text: { r: 0.933, g: 0.949, b: 1.0 },
  dim: { r: 0.545, g: 0.584, b: 0.659 },
  outline: { r: 0.165, g: 0.208, b: 0.282 },
};

function solid(c, a = 1) {
  return [{ type: 'SOLID', color: { r: c.r, g: c.g, b: c.b }, opacity: a }];
}

let maxX = 0;
for (const child of figma.currentPage.children) {
  maxX = Math.max(maxX, child.x + child.width);
}

const frame = figma.createAutoLayout('VERTICAL');
frame.name = 'UI Guide — Spotlight';
frame.resize(480, 400);
frame.x = maxX + 120;
frame.y = 0;
frame.paddingLeft = frame.paddingRight = frame.paddingTop = frame.paddingBottom = 0;
frame.primaryAxisAlignItems = 'CENTER';
frame.counterAxisAlignItems = 'CENTER';
frame.fills = solid(C.void);
frame.clipsContent = true;

const header = figma.createAutoLayout('HORIZONTAL');
header.name = 'Header';
header.resize(480, 48);
header.layoutSizingHorizontal = 'FILL';
header.paddingLeft = header.paddingRight = 20;
header.paddingTop = header.paddingBottom = 12;
header.primaryAxisAlignItems = 'CENTER';
header.counterAxisAlignItems = 'CENTER';
header.fills = solid(C.panel);
frame.appendChild(header);

const title = figma.createText();
title.fontName = { family: 'Inter', style: 'Bold' };
title.characters = 'UI Guide — Spotlight';
title.fontSize = 14;
title.fills = solid(C.teal);
header.appendChild(title);

const stage = figma.createFrame();
stage.name = 'Stage';
stage.resize(480, 352);
stage.layoutSizingHorizontal = 'FILL';
stage.fills = solid(C.void);
frame.appendChild(stage);

// Dim overlay
const dim = figma.createRectangle();
dim.name = 'Dim overlay';
dim.resize(480, 352);
dim.fills = [{ type: 'SOLID', color: { r: 0, g: 0, b: 0 }, opacity: 0.62 }];
stage.appendChild(dim);

// Mock toolbar chip (target)
const chip = figma.createAutoLayout('HORIZONTAL');
chip.name = 'Target — Mixer';
chip.resize(88, 36);
chip.x = 196;
chip.y = 148;
chip.cornerRadius = 8;
chip.paddingLeft = chip.paddingRight = 14;
chip.paddingTop = chip.paddingBottom = 8;
chip.fills = solid(C.glass);
chip.strokes = solid(C.teal, 0.9);
chip.strokeWeight = 2;
stage.appendChild(chip);

const chipLabel = figma.createText();
chipLabel.fontName = { family: 'Inter', style: 'Medium' };
chipLabel.characters = 'Mixer';
chipLabel.fontSize = 12;
chipLabel.fills = solid(C.text);
chip.appendChild(chipLabel);

// Spotlight rings
for (let i = 0; i < 3; i++) {
  const ring = figma.createEllipse();
  ring.name = `Pulse ring ${i + 1}`;
  const size = 120 + i * 36;
  ring.resize(size, size);
  ring.x = 240 - size / 2;
  ring.y = 166 - size / 2;
  ring.fills = [];
  ring.strokes = solid(C.teal, 0.55 - i * 0.15);
  ring.strokeWeight = 2;
  stage.appendChild(ring);
}

// Aurora glow behind hole
const glow = figma.createEllipse();
glow.name = 'Aurora glow';
glow.resize(160, 160);
glow.x = 160;
glow.y = 96;
glow.fills = solid(C.violet, 0.22);
glow.effects = [{
  type: 'LAYER_BLUR',
  radius: 48,
  visible: true,
}];
stage.insertChild(2, glow);

// Callout preview
const callout = figma.createAutoLayout('VERTICAL');
callout.name = 'Callout';
callout.resize(300, 96);
callout.x = 90;
callout.y = 228;
callout.cornerRadius = 10;
callout.paddingLeft = callout.paddingRight = 14;
callout.paddingTop = 10;
callout.paddingBottom = 10;
callout.itemSpacing = 4;
callout.fills = solid(C.glass);
callout.strokes = solid(C.outline);
callout.strokeWeight = 1;
stage.appendChild(callout);

const ct = figma.createText();
ct.fontName = { family: 'Inter', style: 'Bold' };
ct.characters = 'Back to arrange';
ct.fontSize = 13;
ct.fills = solid(C.text);
callout.appendChild(ct);

const cb = figma.createText();
cb.fontName = { family: 'Inter', style: 'Regular' };
cb.characters = 'Mixer is one click away — B or the Mixer button.';
cb.fontSize = 11;
cb.fills = solid(C.dim);
cb.textAutoResize = 'HEIGHT';
cb.resize(272, 32);
callout.appendChild(cb);

frame.setSharedPluginData('dsb', 'key', 'ui-guide/spotlight');

return { createdNodeIds: [frame.id, stage.id, chip.id, callout.id], frameId: frame.id };
