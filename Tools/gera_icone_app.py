# AURA Preamp Bass — gera Resources/icon/icon.png (ícone quadrado do app) a partir
# dos próprios assets do painel (knob.png + pointer.png), sobre um fundo escuro liso
# (sem a textura de metal) e o anel de acento laranja da interface. É o ícone usado
# pelo app nativo (CMakeLists.txt: ICON_BIG/ICON_SMALL) e, redimensionado, pelo site
# (Tools/build-web.sh). Rode de novo sempre que o knob ou o acento de cor mudar.
# Uso: python3 Tools/gera_icone_app.py
#
# Não arredonda os cantos aqui: o macOS (e o Windows) já aplica o próprio
# arredondamento/máscara ao ícone do app a partir de um quadrado cheio.

import math
from PIL import Image, ImageDraw

S = 1024
cx, cy = S // 2, S // 2
ACCENT = (232, 137, 12, 255)
FUNDO = (19, 20, 23, 255)   # mesmo tom escuro da tela de ativação/campos do painel

knob_src = Image.open('Resources/ui/knob.png').convert('RGBA')
pointer_src = Image.open('Resources/ui/pointer.png').convert('RGBA')

canvas = Image.new('RGBA', (S, S), FUNDO)
draw = ImageDraw.Draw(canvas, 'RGBA')

# Anel de destaque (cor de acento do painel), como o arco aceso ao redor dos knobs.
ring_r = int(S * 0.40)
ring_w = int(S * 0.045)
bbox = (cx - ring_r, cy - ring_r, cx + ring_r, cy + ring_r)
draw.arc(bbox, start=125, end=360 + 35, fill=ACCENT, width=ring_w)
for ang in (125, 35):
    rad = math.radians(ang)
    px = cx + ring_r * math.cos(rad)
    py = cy + ring_r * math.sin(rad)
    r = ring_w / 2
    draw.ellipse((px - r, py - r, px + r, py + r), fill=ACCENT)

# O knob (corpo real do plugin), ampliado e centralizado.
kw = int(S * 0.60)
knob = knob_src.resize((kw, kw), Image.LANCZOS)
canvas.alpha_composite(knob, (cx - kw // 2, cy - kw // 2))

# O ponteiro, girado para um ângulo "ligado" (mesmo recurso usado ao vivo no plugin).
pw = int(kw * 0.78)
angulo_graus = 35  # ~2 horas: valor alto, "dirigido"
pointer = pointer_src.resize((pw, pw), Image.LANCZOS).rotate(-angulo_graus, resample=Image.BICUBIC, expand=False)
canvas.alpha_composite(pointer, (cx - pw // 2, cy - pw // 2))

canvas.save('Resources/icon/icon.png')
print('ok', canvas.size)
