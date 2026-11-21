#!/usr/bin/env python3
# AURA Preamp Bass — estende a placa do painel para caber a faixa de efeitos
# Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
# Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
# cópia, redistribuição ou obra derivada apenas com autorização por escrito.
# Licença: ver LICENSE  |  https://github.com/ledixplugins/Drive
#
# A faixa nova não é desenhada: é o próprio metal da placa, recortado de uma
# região limpa e repetido com espelhamento, para a escovação continuar no mesmo
# sentido e sem emenda visível. O chanfro de baixo é o original, recolocado no
# novo fim da placa.
#
# Uso: python3 Tools/estende_painel.py [altura extra em unidades, padrão 84]

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

RAIZ = Path(__file__).resolve().parent.parent
ORIGEM = RAIZ / "Resources/mockup/background_339.jpg"   # a placa original, guardada
DESTINO = RAIZ / "Resources/ui/background.jpg"
ORIGEM_ARCOS = RAIZ / "Resources/mockup/arcs_lit_339.png"   # os arcos acesos, antes da faixa
DESTINO_ARCOS = RAIZ / "Resources/ui/arcs_lit.png"

EXTRA_UN = int(sys.argv[1]) if len(sys.argv) > 1 else 104
ESCALA = 2                      # a arte é 2x das unidades do layout
CHANFRO_PX = 22                 # altura do chanfro de baixo, na arte original
LIMPO = (496, 556)              # faixa de metal sem nada, na arte original


def main():
    if not ORIGEM.exists():
        # primeira vez: guarda a placa original antes de substituí-la
        ORIGEM.parent.mkdir(parents=True, exist_ok=True)
        Image.open(DESTINO).save(ORIGEM, quality=95)
        print(f"placa original guardada em {ORIGEM.relative_to(RAIZ)}")

    base = Image.open(ORIGEM).convert("RGB")
    larg, alt = base.size
    extra = EXTRA_UN * ESCALA

    # A faixa entra no meio da placa, logo acima dos footswitches: assim os
    # footswitches, os parafusos e o chanfro de baixo ficam intactos.
    corte = 530
    nova = Image.new("RGB", (larg, alt + extra))
    nova.paste(base.crop((0, 0, larg, corte)), (0, 0))
    corpo_ate = corte

    # faixa limpa, repetida com espelhamento a cada repetição
    faixa = base.crop((0, LIMPO[0], larg, LIMPO[1]))
    h = faixa.height
    y = corpo_ate
    virada = False
    while y < corpo_ate + extra:
        peca = faixa.transpose(Image.FLIP_TOP_BOTTOM) if virada else faixa
        alto = min(h, corpo_ate + extra - y)
        nova.paste(peca.crop((0, 0, larg, alto)), (0, y))
        y += alto
        virada = not virada

    # todo o resto da placa (footswitches, parafusos, chanfro) desce junto
    nova.paste(base.crop((0, corte, larg, alt)), (0, corte + extra))

    # Sprite de um knob, recortado da própria placa com máscara circular suave,
    # para os controles novos serem iguais aos sete de cima.
    KNOB_CX, KNOB_CY, KNOB_R = 672, 345, 88          # knob do meio (MID), em px
    k = base.crop((KNOB_CX - KNOB_R, KNOB_CY - KNOB_R, KNOB_CX + KNOB_R, KNOB_CY + KNOB_R)).convert("RGBA")
    mascara = Image.new("L", k.size, 0)
    d = ImageDraw.Draw(mascara)
    d.ellipse([2, 2, k.size[0] - 2, k.size[1] - 2], fill=255)
    k.putalpha(mascara.filter(ImageFilter.GaussianBlur(3)))
    k.save(RAIZ / "Resources/ui/knob.png")

    nova.save(DESTINO, quality=92, optimize=True)
    print(f"{DESTINO.relative_to(RAIZ)}: {nova.size[0]}x{nova.size[1]} px "
          f"({nova.size[0] // ESCALA}x{nova.size[1] // ESCALA} unidades)")

    # arcs_lit.png é desenhado esticado para caber o painel inteiro
    # (g.drawImage(..., getLocalBounds())): se ficar menor que o painel, o
    # estiramento desalinha e deforma os arcos acesos dos knobs. Como todo o
    # conteúdo dele fica acima do corte (os sete knobs de cima e o CAB), basta
    # inserir a mesma faixa em branco (transparente) no mesmo lugar.
    if DESTINO_ARCOS.exists():
        if not ORIGEM_ARCOS.exists():
            ORIGEM_ARCOS.parent.mkdir(parents=True, exist_ok=True)
            Image.open(DESTINO_ARCOS).save(ORIGEM_ARCOS)
            print(f"arcos originais guardados em {ORIGEM_ARCOS.relative_to(RAIZ)}")
        arcos = Image.open(ORIGEM_ARCOS).convert("RGBA")
        novos_arcos = Image.new("RGBA", (arcos.width, arcos.height + extra), (0, 0, 0, 0))
        novos_arcos.paste(arcos.crop((0, 0, arcos.width, min(corte, arcos.height))), (0, 0))
        if arcos.height > corte:
            novos_arcos.paste(arcos.crop((0, corte, arcos.width, arcos.height)), (0, corte + extra))
        novos_arcos.save(DESTINO_ARCOS)
        print(f"{DESTINO_ARCOS.relative_to(RAIZ)}: {novos_arcos.size[0]}x{novos_arcos.size[1]} px")


if __name__ == "__main__":
    main()
