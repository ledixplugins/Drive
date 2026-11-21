// AURA Bass Preamp — medidas do painel
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
// cópia, redistribuição ou obra derivada apenas com autorização por escrito.
// Licença: ver LICENSE  |  https://github.com/ledixplugins/Drive
#pragma once

// Geometria do painel do AURA Bass Preamp em unidades de interface (1 unidade = 1 px lógico).
// As imagens em Resources/ui são 2x. As coordenadas vêm da arte de referência
// (Resources/mockup, recortada em Tools/build_ui_assets.py): unidade = (px - recorte) / 2.
namespace AuraLayout
{
    inline constexpr int width  = 726;
    inline constexpr int height = 443;

    // A faixa de efeitos (OCTAVE, CHORUS e o medidor de entrada) foi inserida
    // no meio da placa, logo acima dos footswitches: tudo o que estava abaixo
    // desceu 104 unidades. Ver Tools/estende_painel.py.
    inline constexpr float faixaEfeitosY = 265.0f, faixaEfeitosH = 104.0f;

    struct Knob { const char* paramId; float cx, cy; float valueX; };
    inline constexpr Knob knobs[7] = {
        { "level", 71.50f,   172.50f, 74.5f },
        { "drive", 159.25f,  174.50f, 162.0f },
        { "low", 247.75f,  172.50f, 249.0f },
        { "mid", 336.00f,  172.50f, 336.5f },
        { "high", 424.50f,  172.50f, 423.5f },
        { "presence", 513.50f,  172.75f, 510.5f },
        { "blend", 601.00f,  172.25f, 598.0f },
    };
    inline constexpr float knobHitRadius = 34.0f;
    inline constexpr float knobMinAngleDeg = -135.0f, knobMaxAngleDeg = 135.0f;
    inline constexpr float pointerHalf = 24.0f;      // quadro do indicador (pointer.png) = 48x48
    inline constexpr float valueY = 232.0f;          // centro vertical dos valores
    inline constexpr float valueFontSize = 14.0f;

    // Arcos em volta dos knobs (acendem até o valor)
    inline constexpr float arcCentreDy = 1.5f, arcLitRadius = 46.0f, arcMaxDeg = 142.0f;

    // Footswitch do DRIVE (rótulo ENGAGE) + LED azul
    inline constexpr float driveSwitchX = 655.5f, driveSwitchY = 402.0f;   // canto inferior direito
    inline constexpr float driveSwitchDownHalf = 24.0f;   // sobreposição "pressionado"
    inline constexpr float driveLedX = 616.0f, driveLedY = 402.0f, driveLedHalf = 11.0f;
    struct Rect { float x, y, w, h; };
    inline constexpr Rect driveHit { 554.0f, 378.0f, 126.0f, 48.0f };

    // Footswitch do afinador (TUNER) + LED verde, espelhado no canto inferior esquerdo
    inline constexpr float tunerSwitchX = 70.5f, tunerSwitchY = 402.0f;
    inline constexpr float tunerLedX = 110.0f, tunerLedY = 402.0f;
    inline constexpr Rect tunerHit { 46.0f, 378.0f, 126.0f, 48.0f };
    // Visor do afinador (cobre os knobs enquanto o afinador está ligado)
    inline constexpr Rect tunerDisplay { 30.0f, 98.0f, 666.0f, 156.0f };

    // Botão de menu (presets, tamanho da interface, dicas), no canto superior direito
    inline constexpr Rect menuButton { 662.0f, 40.0f, 26.0f, 22.0f };

    // CAB (rótulo, LED laranja e chave alavanca)
    inline constexpr float cabLedX = 671.35f, cabLedY = 138.65f;
    inline constexpr float cabToggleX = 671.5f, cabToggleY = 175.0f;
    inline constexpr Rect cabHit { 648.0f, 108.0f, 48.0f, 94.0f };

    // Controles da faixa de efeitos: dois knobs menores que os de cima, para
    // ficar claro que são uma segunda seção, e não mais um da fileira principal.
    inline constexpr Knob efeitos[2] = {
        { "octave", 146.0f, 318.0f, 146.0f },
        { "chorus", 274.0f, 318.0f, 274.0f },
    };
    inline constexpr float efeitoRaio = 28.0f;        // raio desenhado do knob
    inline constexpr float efeitoHitRaio = 30.0f;
    inline constexpr float efeitoRotuloY = 276.0f;    // centro do rótulo, acima
    inline constexpr float efeitoValorY = 360.0f;     // centro do valor, abaixo
    inline constexpr float efeitoArcoRaio = 36.0f;

    // Medidor de entrada, à direita dos dois controles. Arrastar nele ajusta o
    // trim; o ponto à direita acende quando o sinal estoura.
    inline constexpr Rect meter { 360.0f, 310.0f, 250.0f, 15.0f };
    inline constexpr Rect meterHit { 352.0f, 298.0f, 290.0f, 42.0f };
    inline constexpr float meterMinDb = -42.0f;     // começo da escala
    inline constexpr float meterClipX = 626.0f;     // centro do ponto de clip
    inline constexpr float meterLabelY = 296.0f;    // linha do rótulo (IN / trim)

    // Sobreposições (meio lado em unidades)
    inline constexpr float ledHalf = 16.0f;
    inline constexpr float toggleHalf = 25.0f;
}
