// Run via Figma MCP use_figma — fileKey ACH9Fh5jq0xP3BDmMQFrXr (after 01)

await figma.loadFontAsync({ family: 'Inter', style: 'Regular' });
await figma.loadFontAsync({ family: 'Inter', style: 'Bold' });
await figma.loadFontAsync({ family: 'Inter', style: 'Medium' });

const C = {
  void: { r: 0.039, g: 0.055, b: 0.078 },
  glass: { r: 0.071, g: 0.094, b: 0.125, a: 0.94 },
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
frame.name = 'UI Guide — Callout';
frame.resize(480, 360);
frame.x = maxX + 80;
frame.y = 0;
frame.fills = solid(C.panel);
frame.clipsContent = true;

const header = figma.createAutoLayout();
header.name = 'Header';
header.resize(480, 44);
header.layoutSizingHorizontal = 'FILL';
header.paddingLeft = 20;
header.paddingTop = 12;
header.fills = solid(C.void);
frame.appendChild(header);

const title = figma.createText();
title.fontName = { family: 'Inter', style: 'Bold' };
title.characters = 'UI Guide — Callout card';
title.fontSize = 14;
title.fills = solid(C.violet);
header.appendChild(title);

const body = figma.createAutoLayout('VERTICAL');
body.name = 'Variants';
body.resize(440, 280);
body.layoutSizingHorizontal = 'FILL';
body.paddingLeft = body.paddingRight = 20;
body.paddingTop = body.paddingBottom = 16;
body.itemSpacing = 16;
body.fills = solid(C.void);
frame.appendChild(body);

function makeCallout(name, titleText, bodyText, accent) {
  const card = figma.createAutoLayout('VERTICAL');
  card.name = name;
  card.resize(440, 108);
  card.layoutSizingHorizontal = 'FILL';
  card.cornerRadius = 10;
  card.paddingLeft = card.paddingRight = 14;
  card.paddingTop = 10;
  card.paddingBottom = 10;
  card.itemSpacing = 6;
  card.fills = solid(C.glass);
  card.strokes = solid(accent, 0.85);
  card.strokeWeight = 1.5;
  card.effects = [{
    type: 'DROP_SHADOW',
    color: { r: accent.r, g: accent.g, b: accent.b, a: 0.35 },
    offset: { x: 0, y: 4 },
    radius: 24,
    spread: 0,
    visible: true,
    blendMode: 'NORMAL',
  }];

  const t = figma.createText();
  t.fontName = { family: 'Inter', style: 'Bold' };
  t.characters = titleText;
  t.fontSize = 13;
  t.fills = solid(C.text);
  card.appendChild(t);

  const b = figma.createText();
  b.fontName = { family: 'Inter', style: 'Regular' };
  b.characters = bodyText;
  b.fontSize = 11;
  b.fills = solid(C.dim);
  b.textAutoResize = 'HEIGHT';
  b.resize(400, 36);
  card.appendChild(b);

  const row = figma.createAutoLayout('HORIZONTAL');
  row.name = 'Actions';
  row.itemSpacing = 8;
  row.primaryAxisAlignItems = 'MAX';
  row.counterAxisAlignItems = 'CENTER';
  row.layoutSizingHorizontal = 'FILL';
  card.appendChild(row);

  for (const [label, primary] of [['Got it', false], ['Next', true]]) {
    const btn = figma.createAutoLayout('HORIZONTAL');
    btn.name = label;
    btn.cornerRadius = 6;
    btn.paddingLeft = btn.paddingRight = 12;
    btn.paddingTop = btn.paddingBottom = 6;
    btn.fills = primary ? solid(accent, 0.25) : [];
    if (!primary) {
      btn.strokes = solid(C.outline);
      btn.strokeWeight = 1;
    }
    const lt = figma.createText();
    lt.fontName = { family: 'Inter', style: 'Medium' };
    lt.characters = label;
    lt.fontSize = 11;
    lt.fills = solid(primary ? accent : C.dim);
    btn.appendChild(lt);
    row.appendChild(btn);
  }

  return card;
}

body.appendChild(makeCallout(
  'Callout / tour step',
  'Transport',
  'Play, stop, record and loop live here. Space toggles playback.',
  C.teal
));

body.appendChild(makeCallout(
  'Callout / panel close',
  'Themes & coach marks',
  'F3 reopens Appearance — place up to three custom pins anywhere.',
  C.violet
));

frame.setSharedPluginData('dsb', 'key', 'ui-guide/callout');

return { createdNodeIds: [frame.id, body.id], frameId: frame.id };
