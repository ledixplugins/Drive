#!/usr/bin/env python3
# AURA Guitar Board — teste de interface e de áudio num Chromium de verdade.
# Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
# Licença: ver LICENSE  |  https://github.com/thiagorgalvao-sketch/AURA
#
# Uso: python3 -I Tests/guitarboard-ui-test.py [pasta do site] [pasta de saída]
# Precisa do Playwright com Chromium instalado.

import http.server
import math
import os
import struct
import sys
import threading
import wave

from playwright.sync_api import sync_playwright

SITE = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "site")
OUT = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else "guitarboard-test")
os.makedirs(OUT, exist_ok=True)

falhas = []


def check(ok, oque):
    print(f"  [{' OK ' if ok else 'FALHA'}] {oque}")
    if not ok:
        falhas.append(oque)


def guitarra_wav(path, hz=220.0, segundos=40, rate=48000):
    """Nota A3 com harmônicos, repetida a cada 2 s (é o que o microfone falso toca)."""
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        frames = bytearray()
        for i in range(int(rate * segundos)):
            t = i / rate
            env = math.exp(-((t % 2.0) * 1.1))
            ph = 2 * math.pi * hz * t
            s = 0.5 * math.sin(ph) + 0.25 * math.sin(2 * ph + 0.4) + 0.12 * math.sin(3 * ph + 1.1)
            frames += struct.pack("<h", int(max(-1, min(1, s * env * 0.5)) * 32000))
        w.writeframes(bytes(frames))


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=SITE, **kw)

    def log_message(self, *a):
        pass


MEDE = """
async (ms) => {
  const ctx = window.__pb.audio, rig = window.__pb.rig;
  if (!ctx || !rig) return -1;
  const an = ctx.createAnalyser();
  an.fftSize = 2048;
  rig.saida.connect(an);
  const buf = new Float32Array(an.fftSize);
  let pico = 0;
  const ate = performance.now() + ms;
  while (performance.now() < ate) {
    an.getFloatTimeDomainData(buf);
    for (let i = 0; i < buf.length; i++) pico = Math.max(pico, Math.abs(buf[i]));
    await new Promise((r) => setTimeout(r, 30));
  }
  rig.saida.disconnect(an);
  return pico;
}
"""

# Energia fora da fundamental e dos harmônicos naturais da nota (220, 440, 660 Hz):
# quanto mais saturação, mais harmônicos novos aparecem (880, 1100, 1320...).
HARMONICOS = """
async () => {
  const ctx = window.__pb.audio, rig = window.__pb.rig;
  const an = ctx.createAnalyser();
  an.fftSize = 16384; an.smoothingTimeConstant = 0.5;
  rig.saida.connect(an);
  await new Promise((r) => setTimeout(r, 900));
  const d = new Float32Array(an.frequencyBinCount);
  an.getFloatFrequencyData(d);
  rig.saida.disconnect(an);
  const bin = (hz) => Math.round(hz / (ctx.sampleRate / an.fftSize));
  const nivel = (hz) => { const b = bin(hz); let m = -200; for (let i = b - 2; i <= b + 2; i++) m = Math.max(m, d[i]); return m; };
  const novos = [880, 1100, 1320, 1540, 1760].map(nivel);
  return { fund: nivel(220), novos: Math.max(...novos) };
}
"""


def main():
    wav = os.path.join(OUT, "guitarra-a3.wav")
    guitarra_wav(wav)

    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 8732), Handler)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    url = "http://127.0.0.1:8732/guitarboard/index.html"

    with sync_playwright() as pw:
        browser = pw.chromium.launch(args=[
            "--no-sandbox",
            "--use-fake-ui-for-media-stream",
            "--use-fake-device-for-media-stream",
            f"--use-file-for-fake-audio-capture={wav}",
            "--autoplay-policy=no-user-gesture-required",
        ])
        page = browser.new_page(viewport={"width": 1280, "height": 860}, device_scale_factor=2)

        erros = []
        page.on("console", lambda m: erros.append(m.text) if m.type == "error" else None)
        page.on("pageerror", lambda e: erros.append(str(e)))
        falhas404 = []
        page.on("response", lambda r: falhas404.append(r.url) if r.status >= 400 else None)

        print("Carregamento da página")
        page.goto(url, wait_until="networkidle")
        page.wait_for_timeout(500)
        check(page.is_visible("#inicio"), "tela inicial aparece")
        check(len(page.query_selector_all(".preset")) == 8, "os oito presets aparecem")
        check(len(page.query_selector_all(".pedal")) == 5, "os cinco pedais aparecem")
        nomes = page.eval_on_selector_all(".pedal h2", "els => els.map(e => e.textContent)")
        check(nomes == ["OVERDRIVE", "DISTORTION", "DETUNE", "CHORUS", "DELAY"], f"ordem dos pedais: {nomes}")
        check(not falhas404, f"nenhum recurso 4xx: {falhas404[:3]}")
        check(not erros, f"sem erros no console: {erros[:2]}")
        page.screenshot(path=f"{OUT}/1-inicio.png")

        print("Escolha da entrada antes de ligar")
        page.click("#botaoLigar")
        page.wait_for_timeout(1500)
        check(page.is_visible("#escolhaEntrada"), "a lista de entradas aparece")
        check(page.evaluate("() => document.getElementById('inicio').hidden") is False,
              "o áudio NÃO começa sozinho no microfone do aparelho")

        print("Início do áudio")
        escolhido = page.eval_on_selector_all("#entradaSelect option", "els => els.filter(e => e.value)[0].value")
        page.select_option("#entradaSelect", escolhido)
        page.click("#entrarButton")
        page.wait_for_timeout(2500)
        check(page.evaluate("() => document.getElementById('inicio').hidden"), "áudio iniciou")
        estado_ctx = page.evaluate("() => window.__pb.audio && window.__pb.audio.state")
        check(estado_ctx == "running", f"contexto de áudio rodando ({estado_ctx})")
        page.screenshot(path=f"{OUT}/2-pedalboard.png")

        print("Cada preset liga os pedais certos")
        esperado = {
            "limpo": ["chorus", "delay"],
            "crunch": ["overdrive"],
            "blues": ["overdrive", "delay"],
            "rock": ["distortion"],
            "pesado": ["overdrive", "distortion"],
            "shoegaze": ["distortion", "detune", "chorus", "delay"],
            "ambiente": ["detune", "chorus", "delay"],
            "tudo": ["overdrive", "distortion", "detune", "chorus", "delay"],
        }
        for pid, cadeia in esperado.items():
            page.evaluate("(id) => window.__pb.aplicaPreset(id)", pid)
            page.wait_for_timeout(120)
            ligados = page.evaluate(
                "() => Object.entries(window.__pb.estado.pedais).filter(([,v]) => v.on).map(([k]) => k)")
            check(sorted(ligados) == sorted(cadeia), f"preset {pid}: {', '.join(cadeia)}")

        print("Todos os presets produzem som e respeitam 0 dBFS")
        picos = {}
        for pid in esperado:
            page.evaluate("(id) => window.__pb.aplicaPreset(id)", pid)
            page.wait_for_timeout(500)
            picos[pid] = page.evaluate(MEDE, 1200)
            print(f"   {pid}: pico {picos[pid]:.3f}")
        check(all(v > 0.005 for v in picos.values()), "todos os presets produzem som")
        check(all(v <= 1.0 for v in picos.values()), "nenhum preset passa de 0 dBFS na saída")

        print("A saturação cria harmônicos novos")
        page.evaluate("(id) => window.__pb.aplicaPreset(id)", "limpo")
        page.evaluate("() => { window.__pb.estado.pedais.delay.on = false; window.__pb.estado.pedais.chorus.on = false; }")
        page.evaluate("() => { for (const [k, b] of Object.entries(window.__pb.rig.blocos)) b.bypass(false); }")
        page.wait_for_timeout(500)
        limpo = page.evaluate(HARMONICOS)
        page.evaluate("(id) => window.__pb.aplicaPreset(id)", "rock")
        page.wait_for_timeout(600)
        sujo = page.evaluate(HARMONICOS)
        print(f"   novos harmônicos: limpo {limpo['novos']:.1f} dB, rock {sujo['novos']:.1f} dB")
        check(sujo["novos"] - sujo["fund"] > limpo["novos"] - limpo["fund"] + 10,
              "distorção acrescenta harmônicos (≥10 dB acima do som limpo)")

        print("Detune desloca a altura")
        page.evaluate("(id) => window.__pb.aplicaPreset(id)", "ambiente")
        page.evaluate("""() => {
          for (const k of ['chorus', 'delay']) window.__pb.rig.blocos[k].bypass(false);
          window.__pb.poeValor('detune', 'cents', 50);
          window.__pb.poeValor('detune', 'mix', 100);
        }""")
        page.wait_for_timeout(1500)
        desloc = page.evaluate("""
        async () => {
          const ctx = window.__pb.audio, rig = window.__pb.rig;
          const an = ctx.createAnalyser();
          an.fftSize = 32768; an.smoothingTimeConstant = 0.6;
          rig.saida.connect(an);
          await new Promise((r) => setTimeout(r, 1200));
          const d = new Float32Array(an.frequencyBinCount);
          an.getFloatFrequencyData(d);
          rig.saida.disconnect(an);
          const hz = (b) => b * ctx.sampleRate / an.fftSize;
          let m = -999, mb = 0;
          for (let b = Math.floor(200 / (ctx.sampleRate / an.fftSize)); b < 260 / (ctx.sampleRate / an.fftSize); b++)
            if (d[b] > m) { m = d[b]; mb = b; }
          return hz(mb);
        }
        """)
        alvo = 220 * 2 ** (50 / 1200)
        print(f"   fundamental: {desloc:.2f} Hz (esperado {alvo:.2f} Hz)")
        check(abs(1200 * math.log2(desloc / alvo)) < 8, "a fundamental sobe ~50 cents com o detune a 100%")

        print("Footswitch e knobs respondem")
        page.evaluate("(id) => window.__pb.aplicaPreset(id)", "crunch")
        page.wait_for_timeout(200)
        page.click('[data-pedal="overdrive"] .foot')
        page.wait_for_timeout(150)
        check(page.evaluate("() => window.__pb.estado.pedais.overdrive.on") is False, "footswitch desliga o pedal")
        page.click('[data-pedal="overdrive"] .foot')
        page.wait_for_timeout(150)
        check(page.evaluate("() => window.__pb.estado.pedais.overdrive.on") is True, "footswitch liga de novo")

        knob = page.locator('[data-pedal="overdrive"] [data-knob="drive"]')
        caixa = knob.bounding_box()
        antes = page.evaluate("() => window.__pb.estado.pedais.overdrive.params.drive")
        page.mouse.move(caixa["x"] + caixa["width"] / 2, caixa["y"] + caixa["height"] / 2)
        page.mouse.down()
        page.mouse.move(caixa["x"] + caixa["width"] / 2, caixa["y"] + caixa["height"] / 2 - 40, steps=8)
        page.mouse.up()
        depois = page.evaluate("() => window.__pb.estado.pedais.overdrive.params.drive")
        print(f"   DRIVE: {antes:.0f} -> {depois:.0f}")
        check(depois > antes, "arrastar o knob aumenta o valor")
        page.mouse.dblclick(caixa["x"] + caixa["width"] / 2, caixa["y"] + caixa["height"] / 2)
        page.wait_for_timeout(150)
        volta = page.evaluate("() => window.__pb.estado.pedais.overdrive.params.drive")
        check(volta == 35, f"toque duplo volta ao valor original ({volta})")

        print("Menu e estado salvo")
        page.click("#botaoMenu")
        page.wait_for_timeout(200)
        check(not page.evaluate("() => document.getElementById('menu').hidden"), "menu abre")
        page.wait_for_timeout(900)
        sels = page.eval_on_selector_all(
            "#menu select", "els => els.map(e => [...e.options].map(o => o.textContent))")
        check(len(sels) == 2, "menu traz entrada e saída de áudio")
        page.keyboard.press("Escape")
        page.keyboard.press("6")
        page.wait_for_timeout(300)
        check(page.evaluate("() => window.__pb.estado.preset") == "shoegaze", "tecla 6 escolhe o sexto preset")
        page.screenshot(path=f"{OUT}/3-shoegaze.png")
        page.reload(wait_until="networkidle")
        page.wait_for_timeout(600)
        check(page.evaluate("() => window.__pb.estado.preset") == "shoegaze", "preset volta após recarregar")

        print("Telas pequenas")
        for nome, w, h in [("celular", 390, 844), ("tablet", 1024, 768)]:
            page.set_viewport_size({"width": w, "height": h})
            page.wait_for_timeout(300)
            medidas = page.evaluate("""() => {
                const b = document.getElementById('board').getBoundingClientRect();
                const dentro = [...document.querySelectorAll('.pedal')].every((p) => {
                  const r = p.getBoundingClientRect();
                  return r.left >= -1 && r.right <= window.innerWidth + 1;
                });
                const knobsLinha = [...document.querySelectorAll('[data-pedal="delay"] .knob')]
                  .map((k) => Math.round(k.getBoundingClientRect().top));
                return { cabe: b.left >= -1 && b.right <= window.innerWidth + 1 && dentro,
                         semRolagem: document.documentElement.scrollWidth <= window.innerWidth + 1,
                         umaLinha: new Set(knobsLinha).size === 1 };
            }""")
            check(medidas["cabe"], f"pedais cabem na tela ({nome}: {w}x{h})")
            check(medidas["semRolagem"], f"sem rolagem horizontal ({nome})")
            check(medidas["umaLinha"], f"os quatro knobs do delay ficam numa linha ({nome})")
            page.screenshot(path=f"{OUT}/4-{nome}.png", full_page=True)

        check(not erros, f"sem erros no console ao final: {erros[:3]}")
        browser.close()

    srv.shutdown()
    print("\n" + ("Todos os testes passaram." if not falhas else f"{len(falhas)} teste(s) falharam."))
    return 1 if falhas else 0


if __name__ == "__main__":
    sys.exit(main())
