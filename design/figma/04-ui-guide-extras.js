// Run via Figma MCP use_figma — Aurora, Ripple, full workspace composite
// fileKey ACH9Fh5jq0xP3BDmMQFrXr (after 03)

await figma.loadFontAsync({ family: 'Inter', style: 'Regular' });
await figma.loadFontAsync({ family: 'Inter', style: 'Bold' });

const C = {
  void: { r: 0.039, g: 0.055, b: 0.078 },
  glass: { r: 0.071, g: 0.094, b: 0.125 },
  panel: { r: 0.102, g: 0.133, b: 0.188 },
  teal: { r: 0.176, g: 0.831, b: 0.749 },
  violet: { r: 0.655, g: 0.545, b: 0.980 },
  amber: { r: 0.961, g: 0.620, b: 0.043 },
  text: { r: 0.933, g: 0.949, b: 1.0 },
  dim: { r: 0.545, g: 0.584, b: 0.659 },
};

function solid(c, a = 1) {
  return [{ type: 'SOLID', color: { r: c.r, g: c.g, b: c.b }, opacity: a }];
}

let maxX = 0;
let maxY = 0;
for (const child of figma.currentPage.children) {
  maxX = Math.max(maxX, child.x + child.width);
  maxY = Math.max(maxY, child.y + child.height);
}

const createdNodeIds = [];

// --- Aurora + Ripple atoms (horizontal strip) ---
const atoms = figma.createAutoLayout('HORIZONTAL');
atoms.name = 'UI Guide — Effect atoms';
atoms.resize(640, 200);
atoms.x = maxX + 80;
atoms.y = 0;
atoms.itemSpacing = 24;
atoms.paddingLeft = atoms.paddingRight = 20;
atoms.paddingTop = atoms.paddingBottom = 16;
atoms.fills = solid(C.panel);
atoms.cornerRadius = 12;

const atomTitle = figma.createText();
atomTitle.fontName = { family: 'Inter', style: 'Bold' };
atomTitle.characters = 'Aurora · Ripple · Shimmer';
atomTitle.fontSize = 12;
atomTitle.fills = solid(C.teal);
// title goes in first cell - use wrapper
const titleRow = figma.createAutoLayout('VERTICAL');
titleRow.resize(600, 168);
titleRow.itemSpacing = 12;
titleRow.appendChild(atomTitle);

const row = figma.createAutoLayout('HORIZONTAL');
row.itemSpacing = 20;
titleRow.appendChild(row);

for (const [name, color] of [['Aurora', C.violet], ['Ripple', C.teal], ['Shimmer', C.amber]]) {
  const cell = figma.createFrame();
  cell.name = name;
  cell.resize(120, 120);
  cell.fills = solid(C.void);
  cell.cornerRadius = 8;
  const orb = figma.createEllipse();
  orb.resize(72, 72);
  orb.x = 24;
  orb.y = 24;
  orb.fills = solid(color, 0.35);
  orb.effects = [{ type: 'LAYER_BLUR', radius: 32, visible: true }];
  cell.appendChild(orb);
  for (let i = 0; i < 2; i++) {
    const ring = figma.createEllipse();
    const s = 80 + i * 24;
    ring.resize(s, s);
    ring.x = 60 - s / 2;
    ring.y = 60 - s / 2;
    ring.fills = [];
    ring.strokes = solid(color, 0.5 - i * 0.2);
    ring.strokeWeight = 1.5;
    cell.appendChild(ring);
  }
  row.appendChild(cell);
}

atoms.appendChild(titleRow);
createdNodeIds.push(atoms.id);

// --- Full workspace composite ---
const composite = figma.createFrame();
composite.name = 'UI Guide — Workspace overlay';
composite.resize(1440, 900);
composite.x = 0;
composite.y = maxY + 120;
composite.fills = solid(C.void);
composite.clipsContent = true;

// Aurora background blobs
for (const [cx, cy, col, a] of [[200, 120, C.violet, 0.18], [1100, 200, C.teal, 0.14], [800, 700, C.amber, 0.1]]) {
  const blob = figma.createEllipse();
  blob.resize(420, 420);
  blob.x = cx - 210;
  blob.y = cy - 210;
  blob.fills = solid(col, a);
  blob.effects = [{ type: 'LAYER_BLUR', radius: 80, visible: true }];
  composite.appendChild(blob);
}

const dim = figma.createRectangle();
dim.resize(1440, 900);
dim.fills = [{ type: 'SOLID', color: { r: 0, g: 0, b: 0 }, opacity: 0.55 }];
composite.appendChild(dim);

// Mock transport
const tbar = figma.createRectangle();
tbar.name = 'Transport';
tbar.resize(1440, 56);
tbar.fills = solid(C.panel);
composite.appendChild(tbar);

const arrange = figma.createRectangle();
arrange.name = 'Arrange';
arrange.resize(1440, 820);
arrange.y = 56;
arrange.fills = solid(C.glass, 0.4);
composite.appendChild(arrange);

// Highlight arrange + callout
const hole = figma.createRectangle();
hole.name = 'Spotlight hole';
hole.resize(900, 400);
hole.x = 270;
hole.y = 200;
hole.cornerRadius = 16;
hole.fills = [];
hole.strokes = solid(C.teal);
hole.strokeWeight = 2;
composite.appendChild(hole);

const bigCallout = figma.createAutoLayout('VERTICAL');
bigCallout.name = 'Tour callout';
bigCallout.resize(340, 100);
bigCallout.x = 550;
bigCallout.y = 620;
bigCallout.cornerRadius = 10;
bigCallout.paddingLeft = bigCallout.paddingRight = 16;
bigCallout.paddingTop = 12;
bigCallout.paddingBottom = 12;
bigCallout.itemSpacing = 4;
bigCallout.fills = solid(C.glass, 0.95);
bigCallout.strokes = solid(C.violet, 0.7);
bigCallout.strokeWeight = 1;
composite.appendChild(bigCallout);

const bt = figma.createText();
bt.fontName = { family: 'Inter', style: 'Bold' };
bt.characters = 'Arrange';
bt.fontSize = 14;
bt.fills = solid(C.text);
bigCallout.appendChild(bt);

const bb = figma.createText();
bb.fontName = { family: 'Inter', style: 'Regular' };
bb.characters = 'Clips, comping and swipe crossfades live on the timeline lanes.';
bb.fontSize = 12;
bb.fills = solid(C.dim);
bb.textAutoResize = 'HEIGHT';
bb.resize(300, 40);
bigCallout.appendChild(bb);

composite.setSharedPluginData('dsb', 'key', 'ui-guide/workspace');
createdNodeIds.push(composite.id);

return { createdNodeIds, atomsId: atoms.id, compositeId: composite.id };
