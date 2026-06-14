// Run via Figma MCP use_figma — fileKey ACH9Fh5jq0xP3BDmMQFrXr (after 02)

await figma.loadFontAsync({ family: 'Inter', style: 'Regular' });
await figma.loadFontAsync({ family: 'Inter', style: 'Bold' });

const C = {
  void: { r: 0.039, g: 0.055, b: 0.078 },
  glass: { r: 0.071, g: 0.094, b: 0.125, a: 0.92 },
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
for (const child of figma.currentPage.children) {
  maxX = Math.max(maxX, child.x + child.width);
}

const frame = figma.createAutoLayout('VERTICAL');
frame.name = 'UI Guide — Fly-beam';
frame.resize(520, 420);
frame.x = maxX + 80;
frame.y = 0;
frame.fills = solid(C.void);
frame.clipsContent = true;

const header = figma.createAutoLayout();
header.name = 'Header';
header.resize(520, 44);
header.layoutSizingHorizontal = 'FILL';
header.paddingLeft = 20;
header.paddingTop = 12;
header.fills = solid(C.panel);
frame.appendChild(header);

const title = figma.createText();
title.fontName = { family: 'Inter', style: 'Bold' };
title.characters = 'UI Guide — Fly-beam (panel close)';
title.fontSize = 14;
title.fills = solid(C.amber);
header.appendChild(title);

const stage = figma.createFrame();
stage.name = 'Stage';
stage.resize(520, 376);
stage.layoutSizingHorizontal = 'FILL';
stage.fills = solid(C.void);
frame.appendChild(stage);

// Mini workspace mock
const bar = figma.createRectangle();
bar.name = 'Transport bar';
bar.resize(520, 52);
bar.fills = solid(C.panel);
stage.appendChild(bar);

const content = figma.createRectangle();
content.name = 'Arrange area';
content.resize(520, 324);
content.y = 52;
content.fills = solid(C.glass, 0.5);
stage.appendChild(content);

// Closed panel ghost (centre)
const ghost = figma.createAutoLayout('VERTICAL');
ghost.name = 'Closed panel origin';
ghost.resize(200, 120);
ghost.x = 160;
ghost.y = 130;
ghost.cornerRadius = 12;
ghost.paddingTop = ghost.paddingBottom = 12;
ghost.paddingLeft = ghost.paddingRight = 16;
ghost.fills = solid(C.panel, 0.35);
ghost.strokes = solid(C.violet, 0.4);
ghost.strokeWeight = 1;
ghost.effects = [{ type: 'LAYER_BLUR', radius: 6, visible: true }];
stage.appendChild(ghost);

const ghostLabel = figma.createText();
ghostLabel.fontName = { family: 'Inter', style: 'Regular' };
ghostLabel.characters = 'Appearance\n(closed)';
ghostLabel.fontSize = 11;
ghostLabel.fills = solid(C.dim);
ghostLabel.textAlignHorizontal = 'CENTER';
ghost.appendChild(ghostLabel);

// Target Theme button
const themeBtn = figma.createAutoLayout('HORIZONTAL');
themeBtn.name = 'Target — Theme';
themeBtn.resize(72, 32);
themeBtn.x = 360;
themeBtn.y = 10;
themeBtn.cornerRadius = 6;
themeBtn.paddingLeft = themeBtn.paddingRight = 10;
themeBtn.fills = solid(C.glass);
themeBtn.strokes = solid(C.teal);
themeBtn.strokeWeight = 2;
stage.appendChild(themeBtn);

const themeTxt = figma.createText();
themeTxt.fontName = { family: 'Inter', style: 'Medium' };
themeTxt.characters = 'Theme';
themeTxt.fontSize = 11;
themeTxt.fills = solid(C.teal);
themeBtn.appendChild(themeTxt);

// Fly-beam vector (SVG import)
const beamSvg = figma.createNodeFromSvg(
  '<svg width="520" height="376" viewBox="0 0 520 376" xmlns="http://www.w3.org/2000/svg">' +
  '<defs><linearGradient id="g" x1="0%" y1="0%" x2="100%" y2="0%">' +
  '<stop offset="0%" stop-color="#A78BFA" stop-opacity="0.2"/>' +
  '<stop offset="100%" stop-color="#2DD4BF" stop-opacity="0.95"/>' +
  '</linearGradient></defs>' +
  '<line x1="260" y1="190" x2="396" y2="26" stroke="url(#g)" stroke-width="3" stroke-linecap="round"/>' +
  '<circle cx="260" cy="190" r="6" fill="#A78BFA"/>' +
  '<circle cx="380" cy="36" r="14" fill="none" stroke="#2DD4BF" stroke-width="2" opacity="0.8"/>' +
  '<circle cx="380" cy="36" r="24" fill="none" stroke="#2DD4BF" stroke-width="1.5" opacity="0.45"/>' +
  '</svg>'
);
beamSvg.name = 'Fly-beam path';
stage.appendChild(beamSvg);

// Legend
const legend = figma.createText();
legend.fontName = { family: 'Inter', style: 'Regular' };
legend.characters = 'Beam travels from closed panel → toolbar anchor when floating windows dismiss.';
legend.fontSize = 10;
legend.fills = solid(C.dim);
legend.x = 20;
legend.y = 340;
legend.resize(480, 28);
stage.appendChild(legend);

frame.setSharedPluginData('dsb', 'key', 'ui-guide/fly-beam');

return { createdNodeIds: [frame.id, stage.id, themeBtn.id, beamSvg.id], frameId: frame.id };
