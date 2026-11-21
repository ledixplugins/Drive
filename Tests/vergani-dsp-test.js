// VERGANI PEDALBOARD — testes do processamento de áudio (Node)
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Licença: ver LICENSE  |  https://github.com/ledixplugins/Drive
// Uso: node Tests/vergani-dsp-test.js
const { DetuneCore } = require('../vergani/gt-processor.js');
const { curvaOverdrive, curvaDistorcao, PICO_CURVA } = require('../vergani/engine.js');
const { PRESETS, PEDAIS, PADROES, ORDEM_PEDAIS } = require('../vergani/presets.js');

const SR = 48000, BLOCK = 128;
let failures = 0;
const check = (ok, what) => { console.log(`  [${ok ? ' OK ' : 'FALHA'}] ${what}`); if (!ok) failures++; };
const db = (x) => 20 * Math.log10(Math.max(1e-12, x));

function roda(core, hz, amp, seconds, cents, mix) {
  const total = Math.round(SR * seconds), out = new Float32Array(total);
  const inb = new Float32Array(BLOCK), ob = new Float32Array(BLOCK);
  for (let pos = 0; pos < total; pos += BLOCK) {
    for (let i = 0; i < BLOCK; i++) inb[i] = amp * Math.sin((2 * Math.PI * hz * (pos + i)) / SR);
    core.process(inb, cents, mix, ob, BLOCK);
    out.set(ob.subarray(0, Math.min(BLOCK, total - pos)), pos);
  }
  return out;
}
const rms = (v, from) => { let s = 0; for (let i = from; i < v.length; i++) s += v[i] * v[i]; return Math.sqrt(s / (v.length - from)); };
function amplitudeEm(v, from, f) {
  let re = 0, im = 0;
  for (let i = from; i < v.length; i++) { const ph = (2 * Math.PI * f * i) / SR; re += v[i] * Math.cos(ph); im += v[i] * Math.sin(ph); }
  return (2 * Math.hypot(re, im)) / (v.length - from);
}
function picoDeFrequencia(v, from, lo, hi, passo) {
  let melhor = lo, max = -1;
  for (let f = lo; f <= hi; f += passo) { const a = amplitudeEm(v, from, f); if (a > max) { max = a; melhor = f; } }
  return melhor;
}

console.log('Detune: a altura muda na quantidade pedida (440 Hz, mix 100%)');
for (const cents of [-50, -25, 14, 50]) {
  const out = roda(new DetuneCore(SR), 440, 0.25, 3, cents, 1);
  const alvo = 440 * Math.pow(2, cents / 1200);
  const achado = picoDeFrequencia(out, SR, alvo - 6, alvo + 6, 0.1);
  const erroCents = 1200 * Math.log2(achado / alvo);
  console.log(`  ${cents >= 0 ? '+' : ''}${cents} ct: esperado ${alvo.toFixed(2)} Hz, medido ${achado.toFixed(2)} Hz (${erroCents.toFixed(2)} ct de erro)`);
  check(Math.abs(erroCents) < 2, `${cents} ct: altura certa (erro < 2 ct)`);
  check(out.every(Number.isFinite), `${cents} ct: sem NaN/Inf`);
  const nivel = db(rms(out, SR) / (0.25 / Math.SQRT2));
  check(Math.abs(nivel) < 3, `${cents} ct: volume parecido com o da entrada (${nivel.toFixed(1)} dB)`);
}

console.log('Detune: mix 0% devolve o sinal original, sem atraso');
{
  const out = roda(new DetuneCore(SR), 330, 0.3, 0.5, 20, 0);
  let erro = 0;
  for (let i = 0; i < out.length; i++) erro = Math.max(erro, Math.abs(out[i] - 0.3 * Math.sin((2 * Math.PI * 330 * i) / SR)));
  check(erro < 1e-5, `erro máximo ${erro.toExponential(1)}`);
}

console.log('Detune: sem estalos nas trocas de janela (sinal contínuo)');
{
  const out = roda(new DetuneCore(SR), 220, 0.3, 4, 40, 1);
  let salto = 0;
  for (let i = SR; i < out.length; i++) salto = Math.max(salto, Math.abs(out[i] - out[i - 1]));
  // senoide de 0,3 a 220 Hz varia no máximo 2π·220/48000·0,3 ≈ 0,0086 por amostra
  console.log(`  maior salto entre amostras: ${salto.toFixed(4)}`);
  check(salto < 0.03, 'a troca entre as duas leituras não faz degrau');
}

console.log('Curvas de saturação');
for (const [nome, fn] of [['overdrive', curvaOverdrive], ['distorção', curvaDistorcao]]) {
  for (const drive of [0, 35, 100]) {
    const c = fn(drive);
    let pico = 0, ok = true;
    for (let i = 0; i < c.length; i++) { pico = Math.max(pico, Math.abs(c[i])); if (!Number.isFinite(c[i])) ok = false; }
    check(ok && pico <= PICO_CURVA + 1e-6, `${nome} drive ${drive}: finita e dentro de ${db(PICO_CURVA).toFixed(0)} dBFS (pico ${pico.toFixed(3)})`);
    let cresce = true;
    for (let i = 1; i < c.length; i++) if (c[i] < c[i - 1] - 1e-6) cresce = false;
    check(cresce, `${nome} drive ${drive}: curva sempre crescente`);
  }
  const fraca = fn(10), forte = fn(90), meio = (fraca.length >> 1) + 40;   // sinal pequeno
  check(Math.abs(forte[meio]) > Math.abs(fraca[meio]), `${nome}: mais drive dá mais ganho em sinal pequeno`);
}
{
  const c = curvaOverdrive(90), n = c.length;
  check(Math.abs(c[n - 1] - PICO_CURVA) < 1e-6 && c[0] < 0 && Math.abs(c[0]) > PICO_CURVA * 0.99,
    'overdrive: os dois lados chegam ao mesmo teto (sem componente contínua)');
}
{
  const c = curvaDistorcao(70), n = c.length;
  const k = 10;                                           // entrada de ±0,01, simétrica em torno do zero
  const pos = c[(n >> 1) + k], neg = c[(n >> 1) - 1 - k];
  check(Math.abs(pos) > Math.abs(neg), 'distorção é assimétrica (harmônicos pares)');
}

console.log('Presets e pedais');
check(PRESETS.length === 8, 'oito presets (teclas 1 a 8)');
check(ORDEM_PEDAIS.join() === 'tuner,overdrive,distortion,detune,chorus,delay', 'ordem fixa dos pedais');
const ids = new Set(PRESETS.map((p) => p.id));
check(ids.size === PRESETS.length, 'ids de preset únicos');
for (const p of PRESETS) {
  const cadeiaOk = p.cadeia.every((c) => ORDEM_PEDAIS.includes(c));
  let ajusteOk = true;
  for (const [pid, vals] of Object.entries(p.ajustes || {})) {
    for (const [k, v] of Object.entries(vals)) {
      const def = (PEDAIS[pid] || { knobs: [] }).knobs.find((x) => x.id === k);
      if (!def || v < def.min || v > def.max) { ajusteOk = false; console.log(`     fora da faixa: ${p.id}.${pid}.${k} = ${v}`); }
    }
  }
  check(cadeiaOk && ajusteOk, `preset ${p.id}: pedais e valores dentro das faixas`);
}
for (const pid of ORDEM_PEDAIS) {
  const todos = PEDAIS[pid].knobs.every((k) => {
    const v = PADROES[pid][k.id];
    return v !== undefined && v >= k.min && v <= k.max;
  });
  check(todos, `pedal ${pid}: todos os knobs têm valor padrão dentro da faixa`);
}

console.log('');
console.log(failures === 0 ? 'Todos os testes passaram.' : `${failures} teste(s) falharam.`);
process.exit(failures === 0 ? 0 : 1);
