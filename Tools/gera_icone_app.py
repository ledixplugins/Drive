# AURA Preamp Bass — gera Resources/icon/icon.png (ícone quadrado do app) a partir
# dos próprios assets do painel (knob.png + pointer.png), sobre uma faixa de metal
# escovado recortada do painel. Rode de novo sempre que o knob ou o acento de cor
# mudar. Uso: python3 Tools/gera_icone_app.py

import math
from PIL import Image, ImageDraw, ImageFilter, ImageOps

S = 1024
cx, cy = S // 2, S // 2

# Fundo: metal escovado escuro, reaproveitando uma faixa limpa do próprio painel,
# ladrilhada para preencher o quadrado (mesmo material do resto da interface).
panel = Image.open('Resources/ui/background.jpg').convert('RGB')
band = panel.crop((40, 170, 1412, 206))  # faixa de metal limpo (sem logotipo nem knobs)
band = band.resize((S, max(1, round(S * band.height / band.width))), Image.LANCZOS)
bg = Image.new('RGB', (S, S))
y = 0
flip = False
while y < S:
    tile = band if not flip else ImageOps.flip(band)
    bg.paste(tile, (0, y))
    y += tile.height
    flip = not flip
bg = bg.filter(ImageFilter.GaussianBlur(1.5))

# Vinheta escura nos cantos, para o ícone não "estourar" contra fundos claros do SO.
vign = Image.new('L', (S, S), 0)
vd = ImageDraw.Draw(vign)
vd.ellipse((-S*0.25, -S*0.25, S*1.25, S*1.25), fill=255)
vign = vign.filter(ImageFilter.GaussianBlur(120))
dark = Image.new('RGB', (S, S), (6, 6, 7))
bg = Image.composite(bg, dark, vign)

canvas = bg.convert('RGBA')
draw = ImageDraw.Draw(canvas, 'RGBA')

# Anel de destaque (cor de acento do painel), como o arco aceso ao redor dos knobs.
ring_r = int(S * 0.40)
ring_w = int(S * 0.045)
accent = (232, 137, 12, 255)
bbox = (cx - ring_r, cy - ring_r, cx + ring_r, cy + ring_r)
draw.arc(bbox, start=125, end=360 + 35, fill=accent, width=ring_w)
# pontas arredondadas do arco
for ang in (125, 35):
    rad = math.radians(ang)
    px = cx + ring_r * math.cos(rad)
    py = cy + ring_r * math.sin(rad)
    r = ring_w / 2
    draw.ellipse((px - r, py - r, px + r, py + r), fill=accent)

# O knob (corpo real do plugin), ampliado e centralizado.
knob = Image.open('Resources/ui/knob.png').convert('RGBA')
kw = int(S * 0.62)
knob = knob.resize((kw, kw), Image.LANCZOS)
canvas.alpha_composite(knob, (cx - kw // 2, cy - kw // 2))

# O ponteiro, girado para um ângulo "ligado" (mesmo recurso usado ao vivo no plugin).
pointer = Image.open('Resources/ui/pointer.png').convert('RGBA')
pw = int(kw * 0.78)
pointer = pointer.resize((pw, pw), Image.LANCZOS)
angulo_graus = 35  # ~2 horas: valor alto, "dirigido"
pointer = pointer.rotate(-angulo_graus, resample=Image.BICUBIC, expand=False)
canvas.alpha_composite(pointer, (cx - pw // 2, cy - pw // 2))

canvas.convert('RGB').save('Resources/icon/icon.png')
print('ok', canvas.size)
