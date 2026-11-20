// AURA Bass — oitava abaixo e chorus
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
// cópia, redistribuição ou obra derivada apenas com autorização por escrito.
// Licença: ver LICENSE  |  https://github.com/aurapluginsaudio/Drive
//
// Os dois blocos são iguais aos da versão web (web/aura-processor.js): mesmas
// constantes, mesma ordem de contas. Mexeu aqui, mexa lá — o teste compara.

#pragma once

#include <cmath>
#include <juce_dsp/juce_dsp.h>

namespace AuraEfeitos
{
    /** Oitava abaixo no estilo dos pedais analógicos.

        Um comparador com histerese acompanha a fundamental, um divisor gera uma
        onda quadrada com metade da frequência e um envelope copia a dinâmica do
        que está sendo tocado. É isso que soa certo num baixo: responde na hora e
        não "borra" a nota, ao contrário de um deslocador de altura por FFT.
    */
    class Oitava
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            aTrack = std::exp (-2.0f * juce::MathConstants<float>::pi * 250.0f / (float) sr);
            aOut   = std::exp (-2.0f * juce::MathConstants<float>::pi * 220.0f / (float) sr);
            aAtk   = std::exp (-1.0f / (0.004f * (float) sr));
            aRel   = std::exp (-1.0f / (0.090f * (float) sr));
            reset();
        }

        void reset()
        {
            lpTrack = 0.0f; env = 0.0f; lp1 = 0.0f; lp2 = 0.0f; dc = 0.0f;
            comp = 1; flip = 1.0f;
        }

        /** Gera a oitava abaixo de uma amostra. Devolve só o sub, sem mistura. */
        float processarAmostra (float x) noexcept
        {
            // Rastreio: só as fundamentais interessam para contar os ciclos.
            lpTrack = aTrack * lpTrack + (1.0f - aTrack) * x;

            const float mag = std::abs (x);
            const float a = mag > env ? aAtk : aRel;
            env = a * env + (1.0f - a) * mag;

            // Histerese proporcional ao envelope: não treme no silêncio.
            const float h = juce::jmax (0.004f, env * 0.12f);
            if (comp <= 0 && lpTrack > h)       { comp = 1;  flip = -flip; }
            else if (comp > 0 && lpTrack < -h)  { comp = -1; }

            float sub = flip * env * 1.6f;

            // Abafa: do sub interessa o fundo, não a quadrada crua.
            lp1 = aOut * lp1 + (1.0f - aOut) * sub;
            lp2 = aOut * lp2 + (1.0f - aOut) * lp1;
            sub = lp2;

            dc = 0.9995f * dc + 0.0005f * sub;
            sub -= dc;

            if (env < 0.001f) sub = 0.0f;       // fecha quando não há nota
            return std::tanh (sub);
        }

    private:
        double sr = 48000.0;
        float aTrack = 0.0f, aOut = 0.0f, aAtk = 0.0f, aRel = 0.0f;
        float lpTrack = 0.0f, env = 0.0f, lp1 = 0.0f, lp2 = 0.0f, dc = 0.0f, flip = 1.0f;
        int comp = 1;
    };

    /** Chorus por atraso modulado, com taxa e profundidade fixas — um botão só,
        do jeito que se usa num baixo: ou tem, ou não tem. */
    class Chorus
    {
    public:
        static constexpr float taxaHz      = 0.33f;    // oscilação lenta
        static constexpr float atrasoBaseMs = 12.0f;
        static constexpr float profundMs   = 3.5f;

        void prepare (double sampleRate, int numChannels)
        {
            sr = sampleRate;
            tamanho = juce::nextPowerOfTwo ((int) (0.064 * sr) + 4);
            linhas.setSize (juce::jmax (1, numChannels), tamanho);
            passoLfo = 2.0f * juce::MathConstants<float>::pi * taxaHz / (float) sr;
            reset();
        }

        void reset()
        {
            linhas.clear();
            escrita = 0;
            fase = 0.0f;
        }

        /** Mistura o chorus no buffer, com a mistura já suavizada por amostra. */
        void processar (juce::AudioBuffer<float>& buffer, int numChannels,
                        juce::SmoothedValue<float>& mistura)
        {
            const int n = buffer.getNumSamples();
            const float baseAm = (float) (atrasoBaseMs * 0.001 * sr);
            const float profAm = (float) (profundMs * 0.001 * sr);
            const int canais = juce::jmin (numChannels, linhas.getNumChannels());

            for (int i = 0; i < n; ++i)
            {
                const float m = mistura.getNextValue();
                const float atraso = baseAm + profAm * (0.5f + 0.5f * std::sin (fase));
                fase += passoLfo;
                if (fase > 2.0f * juce::MathConstants<float>::pi)
                    fase -= 2.0f * juce::MathConstants<float>::pi;

                for (int ch = 0; ch < canais; ++ch)
                {
                    float* linha = linhas.getWritePointer (ch);
                    float* d = buffer.getWritePointer (ch);

                    linha[escrita] = d[i];

                    const float leitura = (float) escrita - atraso;
                    const int i0 = ((int) std::floor (leitura) % tamanho + tamanho) % tamanho;
                    const int i1 = (i0 + 1) % tamanho;
                    const float f = leitura - std::floor (leitura);
                    const float atrasado = linha[i0] * (1.0f - f) + linha[i1] * f;

                    d[i] = d[i] + atrasado * m;   // o seco fica inteiro; o atrasado entra por cima
                }
                escrita = (escrita + 1) % tamanho;
            }
        }

    private:
        double sr = 48000.0;
        int tamanho = 4096, escrita = 0;
        float passoLfo = 0.0f, fase = 0.0f;
        juce::AudioBuffer<float> linhas;
    };
}
