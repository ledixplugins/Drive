// AURA Bass — testes do processamento
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
// cópia, redistribuição ou obra derivada apenas com autorização por escrito.
// Licença: ver LICENSE  |  https://github.com/thiagorgalvao-sketch/AURA
// Teste offline do DSP: roda o processador com senoides e checa níveis, THD,
// estabilidade numérica e o alinhamento do Blend (sem phasing).
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/Licenca.h"
#include <cstdio>

namespace
{
    constexpr double sr = 48000.0;
    constexpr int blockSize = 512;
    constexpr float inputHpfHz = 18.0f;   // igual ao do plugin

    // Senoide em blocos de tamanho variável; devolve o canal 0 (e o 1, se pedido)
    std::vector<float> runBlocks (AuraBassAudioProcessor& p, float freq, float amp, int prepareSize,
                                  const std::vector<int>& sizes, int totalSamples, std::vector<float>* right = nullptr,
                                  int numChannels = 2)
    {
        p.prepareToPlay (sr, prepareSize);
        std::vector<float> out;
        juce::MidiBuffer midi;
        int pos = 0, k = 0;
        while (pos < totalSamples)
        {
            const int n = juce::jmin (sizes[(size_t) (k++ % (int) sizes.size())], totalSamples - pos);
            juce::AudioBuffer<float> buf (numChannels, n);
            buf.clear();
            for (int i = 0; i < n; ++i)
                buf.setSample (0, i, amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (pos + i) / sr));
            if (numChannels > 1 && p.getTotalNumInputChannels() > 1)
                buf.copyFrom (1, 0, buf, 0, 0, n);
            p.processBlock (buf, midi);
            for (int i = 0; i < n; ++i)
            {
                out.push_back (buf.getSample (0, i));
                if (right != nullptr && numChannels > 1) right->push_back (buf.getSample (1, i));
            }
            pos += n;
        }
        return out;
    }

    double maxJump (const std::vector<float>& v, size_t a, size_t b)
    {
        double m = 0;
        for (size_t i = std::max (a, (size_t) 1); i < std::min (b, v.size()); ++i)
            m = std::max (m, (double) std::abs (v[i] - v[i - 1]));
        return m;
    }

    void setParam (AuraBassAudioProcessor& p, const char* id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    // Processa 1 s de senoide e devolve a saída (descartando o primeiro 0,5 s)
    std::vector<float> run (AuraBassAudioProcessor& p, float freq, float amp, std::vector<float>* inputOut = nullptr)
    {
        p.prepareToPlay (sr, blockSize);
        const int total = (int) sr;
        std::vector<float> out, in;
        juce::AudioBuffer<float> buf (2, blockSize);
        juce::MidiBuffer midi;
        for (int start = 0; start < total; start += blockSize)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const float x = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (start + i) / sr);
                buf.setSample (0, i, x);
                buf.setSample (1, i, x);
                in.push_back (x);
            }
            p.processBlock (buf, midi);
            for (int i = 0; i < blockSize; ++i)
                out.push_back (buf.getSample (0, i));
        }
        if (inputOut) *inputOut = in;
        return out;
    }

    double rms (const std::vector<float>& v, size_t from)
    {
        double s = 0; for (size_t i = from; i < v.size(); ++i) s += (double) v[i] * v[i];
        return std::sqrt (s / double (v.size() - from));
    }

    // Amplitude de um harmônico via correlação (Goertzel simplificado)
    double harmonic (const std::vector<float>& v, size_t from, double f)
    {
        double re = 0, im = 0; const size_t n = v.size() - from;
        for (size_t i = 0; i < n; ++i)
        {
            const double ph = 2.0 * juce::MathConstants<double>::pi * f * double (i + from) / sr;
            re += v[i + from] * std::cos (ph); im += v[i + from] * std::sin (ph);
        }
        return 2.0 * std::sqrt (re * re + im * im) / double (n);
    }

    bool finite (const std::vector<float>& v)
    {
        for (auto x : v) if (! std::isfinite (x)) return false;
        return true;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    int failures = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? " OK " : "FALHA", what); if (! ok) ++failures; };

    const size_t from = (size_t) (sr / 2);

    std::printf ("Drive (dB) x nível/THD (senoide 100 Hz, -12 dBFS, cab desligado, EQ plano)\n");
    for (float drive : { -15.0f, -7.5f, 0.0f, 7.5f, 15.0f })
    {
        AuraBassAudioProcessor p;
        setParam (p, ParamIDs::cab, 0.0f);
        setParam (p, ParamIDs::presence, 0.0f);
        setParam (p, ParamIDs::drive, drive);
        auto out = run (p, 100.0f, 0.25f);
        const double h1 = harmonic (out, from, 100), h2 = harmonic (out, from, 200), h3 = harmonic (out, from, 300),
                     h4 = harmonic (out, from, 400), h5 = harmonic (out, from, 500);
        const double thd = std::sqrt (h2*h2 + h3*h3 + h4*h4 + h5*h5) / h1 * 100.0;
        std::printf ("  drive %+5.1f dB  saída %6.1f dBFS  THD %5.1f%%  H2 %5.1f dB  H3 %5.1f dB\n",
                     drive, juce::Decibels::gainToDecibels (rms (out, from) * std::sqrt (2.0)), thd,
                     juce::Decibels::gainToDecibels (h2 / h1), juce::Decibels::gainToDecibels (h3 / h1));
        check (finite (out), "sem NaN/Inf");
    }

    std::printf ("Oitava abaixo e chorus\n");
    {
        auto energiaEm = [] (const std::vector<float>& v, float f)
        {
            double re = 0, im = 0;
            const size_t de = v.size() / 2;
            for (size_t i = de; i < v.size(); ++i)
            {
                const double ph = 2.0 * juce::MathConstants<double>::pi * f * (double) i / 48000.0;
                re += v[i] * std::cos (ph); im += v[i] * std::sin (ph);
            }
            return 2.0 * std::hypot (re, im) / (double) (v.size() - de);
        };

        // Zerados, nada muda: quem tem sessão salva continua ouvindo o mesmo
        {
            AuraBassAudioProcessor a, b2;
            setParam (b2, ParamIDs::octave, 0.0f);
            setParam (b2, ParamIDs::chorus, 0.0f);
            const auto sa = runBlocks (a, 110.0f, 0.2f, blockSize, { blockSize }, 24000);
            const auto sb = runBlocks (b2, 110.0f, 0.2f, blockSize, { blockSize }, 24000);
            float maior = 0.0f;
            for (size_t i = 0; i < sa.size(); ++i) maior = juce::jmax (maior, std::abs (sa[i] - sb[i]));
            check (maior < 1.0e-7f, "com oitava e chorus em 0 a saída é idêntica à de antes");
        }

        // Oitava: aparece energia em 55 Hz a partir de uma nota de 110 Hz
        {
            AuraBassAudioProcessor p;
            setParam (p, ParamIDs::drive, -15.0f);
            setParam (p, ParamIDs::cab, 0.0f);
            setParam (p, ParamIDs::blend, 0.0f);
            setParam (p, ParamIDs::octave, 0.0f);
            const auto sem = runBlocks (p, 110.0f, 0.2f, blockSize, { blockSize }, 48000);
            setParam (p, ParamIDs::octave, 100.0f);
            const auto com = runBlocks (p, 110.0f, 0.2f, blockSize, { blockSize }, 48000);

            const double subSem = energiaEm (sem, 55.0f), subCom = energiaEm (com, 55.0f);
            const double notaCom = energiaEm (com, 110.0f);
            const double ganho = 20.0 * std::log10 ((subCom + 1e-12) / (subSem + 1e-12));
            std::printf ("  55 Hz sobe %.1f dB; sub em relação à nota: %.1f dB\n",
                         ganho, 20.0 * std::log10 ((subCom + 1e-12) / (notaCom + 1e-12)));
            check (ganho > 25.0, "a oitava abaixo aparece uma oitava abaixo da nota");
            check (subCom > 0.25 * notaCom, "a oitava tem peso suficiente para ser ouvida");
            check (notaCom > 0.0, "a nota original continua lá");
        }

        // Oitava não inventa nota no silêncio
        {
            AuraBassAudioProcessor p;
            setParam (p, ParamIDs::octave, 100.0f);
            const auto quieto = runBlocks (p, 110.0f, 0.0f, blockSize, { blockSize }, 24000);
            float pico = 0.0f;
            for (float v : quieto) pico = juce::jmax (pico, std::abs (v));
            check (pico < 1.0e-5f, "sem sinal a oitava fica calada");
        }

        // Chorus: a mistura muda o som e oscila com o tempo
        {
            AuraBassAudioProcessor p;
            setParam (p, ParamIDs::drive, -15.0f);
            setParam (p, ParamIDs::cab, 0.0f);
            setParam (p, ParamIDs::blend, 0.0f);
            setParam (p, ParamIDs::chorus, 0.0f);
            const auto sem = runBlocks (p, 220.0f, 0.2f, blockSize, { blockSize }, 144000);
            setParam (p, ParamIDs::chorus, 100.0f);
            const auto com = runBlocks (p, 220.0f, 0.2f, blockSize, { blockSize }, 144000);

            auto rmsEm = [] (const std::vector<float>& v, size_t de, size_t ate)
            {
                double s = 0;
                for (size_t i = de; i < ate && i < v.size(); ++i) s += (double) v[i] * v[i];
                return std::sqrt (s / (double) (ate - de));
            };
            const double a1 = rmsEm (com, 48000, 60000), a2 = rmsEm (com, 96000, 108000);
            const double variacao = 20.0 * std::log10 ((juce::jmax (a1, a2) + 1e-12) / (juce::jmin (a1, a2) + 1e-12));
            std::printf ("  chorus: variação de nível ao longo do ciclo %.1f dB\n", variacao);
            check (variacao > 0.5, "o chorus varia com o tempo (é modulado)");

            float dif = 0.0f;
            for (size_t i = 100000; i < com.size(); ++i) dif = juce::jmax (dif, std::abs (com[i] - sem[i]));
            check (dif > 0.01f, "o chorus muda o som quando ligado");
        }
    }

    std::printf ("Licença: teste de 15 dias, relógio e ativação\n");
    {
        using namespace AuraLicenca;
        const double dia = umDiaMs;
        const double hoje = 1.7e12;                      // um instante qualquer

        Estado novo;                                     // nunca aberto
        check (avaliar (novo, hoje) == Status::teste, "sem histórico, começa em teste");
        check (diasRestantes (novo, hoje) == 15, "teste começa com 15 dias");

        Estado e; e.t0 = hoje; e.seen = hoje;
        check (diasRestantes (e, hoje + 1 * dia) == 14, "passou um dia, restam 14");
        check (diasRestantes (e, hoje + 14.9 * dia) == 1, "no último dia ainda resta 1");
        check (avaliar (e, hoje + 15.0 * dia) == Status::expirado, "no 15º dia o teste acaba");
        check (! liberado (avaliar (e, hoje + 15.0 * dia)), "expirado não libera o processamento");
        check (liberado (avaliar (e, hoje + 14.0 * dia)), "dentro do teste libera");

        // Atrasar o relógio não devolve dias nem destrava nada
        Estado usado; usado.t0 = hoje; usado.seen = hoje + 20 * dia;
        check (avaliar (usado, hoje + 1 * dia) == Status::relogio, "relógio atrasado bloqueia");
        check (diasRestantes (usado, hoje + 1 * dia) == 0, "relógio atrasado não devolve dias");

        // Adiantar o relógio só gasta o teste mais rápido
        Estado frente; frente.t0 = hoje; frente.seen = hoje;
        check (avaliar (frente, hoje + 400 * dia) == Status::expirado, "adiantar o relógio não estende o teste");

        // Chave ativa: vale offline por 30 dias, depois pede revalidação
        Estado lic; lic.t0 = hoje; lic.seen = hoje; lic.chave = "ABC-123"; lic.instancia = "i1"; lic.val = hoje;
        check (avaliar (lic, hoje + 29 * dia) == Status::licenciada, "licença vale offline por 30 dias");
        check (liberado (avaliar (lic, hoje + 29 * dia)), "licença em dia libera o processamento");
        check (avaliar (lic, hoje + 31 * dia) == Status::revalidar, "passados 30 dias, pede revalidação");
        check (! liberado (avaliar (lic, hoje + 31 * dia)), "revalidação pendente não libera");
        check (avaliar (lic, hoje + 400 * dia) == Status::revalidar, "licença nunca volta a ser teste");

        // Mesclar os dois arquivos: vale a abertura mais antiga
        Estado a1; a1.t0 = hoje + 5 * dia; a1.seen = hoje + 6 * dia;
        Estado a2; a2.t0 = hoje;           a2.seen = hoje + 2 * dia;
        const auto m = mesclar (a1, a2);
        check (m.t0 == hoje, "mesclar mantém a primeira abertura mais antiga");
        check (m.seen == hoje + 6 * dia, "mesclar mantém a maior data vista");
        check (mesclar (Estado{}, a2).t0 == hoje, "arquivo apagado não reinicia o teste");

        Estado comChave; comChave.chave = "K"; comChave.instancia = "i"; comChave.val = hoje + 9 * dia;
        check (mesclar (a1, comChave).temChave(), "mesclar preserva a chave ativada");

        // O gerente grava nos dois lugares e não reinicia ao apagar um deles
        Gerente g;
        auto base = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("aura-lic-teste");
        base.deleteRecursively();
        g.arquivo1 = base.getChildFile ("a/lic.json");
        g.arquivo2 = base.getChildFile ("b/.lic");
        double relogio = hoje;
        g.agora = [&relogio] { return relogio; };
        g.atualizar();
        check (g.status() == Status::teste && g.diasRestantes() == 15, "primeira abertura: 15 dias");
        check (g.arquivo1.existsAsFile() && g.arquivo2.existsAsFile(), "grava o estado nos dois lugares");

        relogio = hoje + 10 * dia;
        g.atualizar();
        check (g.diasRestantes() == 5, "dez dias depois, restam 5");

        g.arquivo1.deleteFile();                       // "reinstalou o plugin"
        g.atualizar();
        check (g.diasRestantes() == 5, "apagar um dos arquivos não reinicia o teste");
        check (g.arquivo1.existsAsFile(), "o arquivo apagado é refeito");

        relogio = hoje;                                // "voltou o relógio"
        g.atualizar();
        check (g.status() == Status::relogio, "voltar o relógio é detectado pelo gerente");
        check (! g.liberado(), "com o relógio atrasado o plugin não processa");

        check (g.idDoAparelho().length() == 32, "o aparelho tem um identificador estável");
        base.deleteRecursively();
    }

    std::printf ("Trim de entrada e medidor\n");
    {
        AuraBassAudioProcessor p;
        setParam (p, ParamIDs::drive, -15.0f);
        setParam (p, ParamIDs::cab, 0.0f);
        setParam (p, ParamIDs::blend, 0.0f);     // só o caminho limpo, para medir ganho puro

        setParam (p, ParamIDs::trim, 0.0f);
        const auto semTrim = runBlocks (p, 110.0f, 0.1f, blockSize, { blockSize }, 24000);
        const float picoSem = p.getInputPeak();

        setParam (p, ParamIDs::trim, 12.0f);
        const auto comTrim = runBlocks (p, 110.0f, 0.1f, blockSize, { blockSize }, 24000);
        const float picoCom = p.getInputPeak();

        auto pico = [] (const std::vector<float>& v)
        {
            float m = 0.0f;
            for (size_t i = v.size() / 2; i < v.size(); ++i) m = juce::jmax (m, std::abs (v[i]));
            return m;
        };
        const float razao = pico (comTrim) / juce::jmax (1.0e-9f, pico (semTrim));
        const float emDb = juce::Decibels::gainToDecibels (razao);
        std::printf ("  trim +12 dB -> saída %+0.2f dB; medidor %.3f -> %.3f\n", emDb, picoSem, picoCom);
        check (std::abs (emDb - 12.0f) < 0.3f, "trim de +12 dB soma 12 dB antes de tudo");
        check (picoCom > picoSem * 3.0f, "o medidor acompanha o trim");
        check (std::abs (picoSem - 0.1f) < 0.02f, "o medidor mostra o pico real da entrada");

        p.consumeInputClip();
        setParam (p, ParamIDs::trim, 24.0f);
        runBlocks (p, 110.0f, 0.3f, blockSize, { blockSize }, 12000);
        check (p.consumeInputClip(), "o aviso de clip acende quando a entrada estoura");
        check (! p.consumeInputClip(), "o aviso de clip apaga depois de lido");
    }

    std::printf ("Blend 0%% = sinal limpo alinhado (sem phasing)\n");
    {
        AuraBassAudioProcessor p;
        setParam (p, ParamIDs::cab, 0.0f);
        setParam (p, ParamIDs::presence, 0.0f);
        setParam (p, ParamIDs::blend, 0.0f);
        setParam (p, ParamIDs::drive, 15.0f);
        std::vector<float> in;
        auto out = run (p, 440.0f, 0.25f, &in);
        const int lat = p.getLatencySamples();
        // Referência: entrada passada pelo mesmo HPF de entrada do plugin
        juce::dsp::IIR::Filter<float> hpf;
        hpf.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, inputHpfHz, 0.707f);
        for (auto& x : in) x = hpf.processSample (x);
        double err = 0, errShift = 0;
        for (size_t i = from; i < out.size(); ++i)
        {
            err      = std::max (err,      (double) std::abs (out[i] - in[i - (size_t) lat]));
            errShift = std::max (errShift, (double) std::abs (out[i] - in[i - (size_t) lat - 1]));
        }
        std::printf ("  (com 1 amostra de desalinhamento o erro seria %.4f)\n", errShift);
        std::printf ("  latência %d amostras, erro máx vs. entrada atrasada: %.4f\n", lat, err);
        check (err < 1.0e-4, "Blend 0% reproduz a entrada sem desalinhamento");
    }

    std::printf ("Cab ligado atenua agudos\n");
    {
        AuraBassAudioProcessor a, b;
        setParam (a, ParamIDs::cab, 0.0f); setParam (b, ParamIDs::cab, 1.0f);
        const double off = rms (run (a, 8000.0f, 0.1f), from), on = rms (run (b, 8000.0f, 0.1f), from);
        std::printf ("  8 kHz: sem cab %.1f dB, com cab %.1f dB\n",
                     juce::Decibels::gainToDecibels (off), juce::Decibels::gainToDecibels (on));
        check (on < off * 0.25, "cab corta pelo menos 12 dB em 8 kHz");
    }

    std::printf ("Botão DRIVE desligado = só EQ/Cab (sinal limpo alinhado)\n");
    {
        AuraBassAudioProcessor p;
        setParam (p, ParamIDs::cab, 0.0f);
        setParam (p, ParamIDs::presence, 0.0f);
        setParam (p, ParamIDs::blend, 100.0f);
        setParam (p, ParamIDs::drive, 15.0f);
        setParam (p, ParamIDs::driveOn, 0.0f);
        std::vector<float> in;
        auto out = run (p, 100.0f, 0.25f, &in);
        juce::dsp::IIR::Filter<float> hpf;
        hpf.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, inputHpfHz, 0.707f);
        for (auto& x : in) x = hpf.processSample (x);
        const int lat = p.getLatencySamples();
        double err = 0;
        for (size_t i = from; i < out.size(); ++i) err = std::max (err, (double) std::abs (out[i] - in[i - (size_t) lat]));
        const double h1 = harmonic (out, from, 100), h3 = harmonic (out, from, 300);
        std::printf ("  erro vs. limpo %.6f, H3 %.1f dB\n", err, juce::Decibels::gainToDecibels (h3 / h1 + 1e-12));
        check (err < 1.0e-4, "DRIVE off ignora Drive +15 dB e Blend 100%");
        check (finite (out), "sem NaN/Inf");
    }

    std::printf ("Botão DRIVE desligado mantém o EQ ativo\n");
    {
        AuraBassAudioProcessor a, b;
        for (auto* p : { &a, &b }) { setParam (*p, ParamIDs::cab, 0.0f); setParam (*p, ParamIDs::presence, 0.0f); setParam (*p, ParamIDs::driveOn, 0.0f); }
        setParam (b, ParamIDs::low, 12.0f);
        const double flat = rms (run (a, 50.0f, 0.1f), from), boosted = rms (run (b, 50.0f, 0.1f), from);
        std::printf ("  50 Hz com Low +12: %+.1f dB\n", juce::Decibels::gainToDecibels (boosted / flat));
        check (boosted > flat * 2.0, "Low continua atuando com o DRIVE desligado");
    }

    std::printf ("Troca de DRIVE no meio do som sem estalo\n");
    {
        AuraBassAudioProcessor p;
        setParam (p, ParamIDs::cab, 0.0f);
        p.prepareToPlay (sr, blockSize);
        juce::AudioBuffer<float> buf (2, blockSize); juce::MidiBuffer midi;
        std::vector<float> out;
        for (int blk = 0; blk < 200; ++blk)
        {
            if (blk == 100) setParam (p, ParamIDs::driveOn, 0.0f);
            for (int i = 0; i < blockSize; ++i)
            {
                const float x = 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 100.0 * (blk * blockSize + i) / sr);
                buf.setSample (0, i, x); buf.setSample (1, i, x);
            }
            p.processBlock (buf, midi);
            for (int i = 0; i < blockSize; ++i) out.push_back (buf.getSample (0, i));
        }
        double maxStep = 0, maxStepAround = 0;
        for (size_t i = 1; i < out.size(); ++i)
        {
            const double d = std::abs (out[i] - out[i - 1]);
            if (i > 20000 && i < 50000) maxStep = std::max (maxStep, d);
            if (i > 51000 && i < 53000) maxStepAround = std::max (maxStepAround, d);
        }
        std::printf ("  maior salto entre amostras: antes %.4f, na troca %.4f\n", maxStep, maxStepAround);
        check (maxStepAround <= maxStep * 1.05, "sem descontinuidade na troca");
    }

    std::printf ("Compensação de volume: DRIVE ligado x desligado (senoide 100 Hz, -12 dBFS)\n");
    for (float drive : { -15.0f, 0.0f, 15.0f })
    {
        AuraBassAudioProcessor on, off;
        for (auto* p : { &on, &off })
        {
            setParam (*p, ParamIDs::cab, 0.0f);
            setParam (*p, ParamIDs::presence, 0.0f);
            setParam (*p, ParamIDs::blend, 100.0f);
            setParam (*p, ParamIDs::drive, drive);
        }
        setParam (off, ParamIDs::driveOn, 0.0f);
        const double a = rms (run (on, 100.0f, 0.25f), from), b = rms (run (off, 100.0f, 0.25f), from);
        const double diff = juce::Decibels::gainToDecibels (a / b);
        std::printf ("  drive %+5.1f dB: diferença %+.1f dB\n", drive, diff);
        check (std::abs (diff) < 1.5, "volume parecido ligado/desligado");
    }

    std::printf ("CAB por resposta de impulso\n");
    {
        AuraBassAudioProcessor a, b;
        for (auto* p : { &a, &b }) { setParam (*p, ParamIDs::presence, 0.0f); setParam (*p, ParamIDs::driveOn, 0.0f); }
        b.setCabIR ("builtin:1");
        const double low4 = rms (run (a, 100.0f, 0.1f), from), hi4 = rms (run (a, 5000.0f, 0.1f), from);
        const double low15 = rms (run (b, 100.0f, 0.1f), from), hi15 = rms (run (b, 5000.0f, 0.1f), from);
        std::printf ("  4x10: 5 kHz %.1f dB abaixo de 100 Hz | 1x15: %.1f dB abaixo\n",
                     juce::Decibels::gainToDecibels (low4 / hi4), juce::Decibels::gainToDecibels (low15 / hi15));
        check (hi15 < hi4, "1x15 mais escura que 4x10");
        {
            AuraBassAudioProcessor c;
            setParam (c, ParamIDs::presence, 0.0f); setParam (c, ParamIDs::driveOn, 0.0f); setParam (c, ParamIDs::cab, 0.0f);
            for (float f : { 80.0f, 300.0f, 1000.0f })
            {
                AuraBassAudioProcessor on; setParam (on, ParamIDs::presence, 0.0f); setParam (on, ParamIDs::driveOn, 0.0f);
                AuraBassAudioProcessor off; setParam (off, ParamIDs::presence, 0.0f); setParam (off, ParamIDs::driveOn, 0.0f); setParam (off, ParamIDs::cab, 0.0f);
                std::printf ("  CAB 4x10 ligado vs desligado em %4.0f Hz: %+.1f dB\n", f,
                             juce::Decibels::gainToDecibels (rms (run (on, f, 0.1f), from) / rms (run (off, f, 0.1f), from)));
            }
        }
        check (b.getCabIRDisplayName() == "1x15", "nome da IR");
        b.setCabIR ("file:/nao/existe.wav");
        check (finite (run (b, 100.0f, 0.1f)), "arquivo inexistente volta para a IR embutida");
    }

    std::printf ("Presets de fábrica\n");
    {
        AuraBassAudioProcessor p;
        check (p.getNumPrograms() == 11, "11 presets (5 de fábrica + 6 medidos do vídeo)");
        p.setCurrentProgram (3);
        const float d = p.apvts.getRawParameterValue (ParamIDs::drive)->load();
        std::printf ("  '%s' -> drive %.1f dB\n", p.getProgramName (3).toRawUTF8(), d);
        check (std::abs (d - 10.0f) < 0.05f, "preset aplica os valores");
        p.setCurrentProgram (1);
        check (p.apvts.getRawParameterValue (ParamIDs::driveOn)->load() < 0.5f, "Clean DI desliga o DRIVE");

        // Os presets de oitava e chorus precisam mexer nos controles novos
        p.setCurrentProgram (10);
        const float cho = p.apvts.getRawParameterValue (ParamIDs::chorus)->load();
        p.setCurrentProgram (9);
        const float oct = p.apvts.getRawParameterValue (ParamIDs::octave)->load();
        std::printf ("  '%s' -> oitava %.0f%% | chorus %.0f%%\n", p.getProgramName (9).toRawUTF8(), oct, cho);
        check (oct > 99.0f && cho > 60.0f, "presets carregam oitava e chorus");
        p.setCurrentProgram (0);
        check (p.apvts.getRawParameterValue (ParamIDs::octave)->load() < 0.5f
                   && p.apvts.getRawParameterValue (ParamIDs::chorus)->load() < 0.5f,
               "preset sem efeito zera oitava e chorus");
    }

    std::printf ("Estado (IR e escala) salvo e restaurado\n");
    {
        AuraBassAudioProcessor a, b;
        a.setCabIR ("builtin:1");
        a.setUiScale (1.25f);
        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        check (b.getCabIR() == "builtin:1" && std::abs (b.getUiScale() - 1.25f) < 0.001f, "IR e escala restauradas");
    }

    std::printf ("Mono\n");
    {
        AuraBassAudioProcessor p;
        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add (juce::AudioChannelSet::mono());
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        check (p.setBusesLayout (mono), "layout mono aceito");
        p.prepareToPlay (sr, blockSize);
        juce::AudioBuffer<float> buf (1, blockSize); juce::MidiBuffer midi;
        for (int i = 0; i < blockSize; ++i) buf.setSample (0, i, 0.5f * (float) std::sin (i * 0.05));
        p.processBlock (buf, midi);
        check (std::isfinite (buf.getSample (0, blockSize - 1)), "processa mono");
    }

    std::printf ("Blocos maiores que o anunciado (prepare 256, blocos de 4096 e tamanhos irregulares)\n");
    {
        AuraBassAudioProcessor a, b;
        const auto ref = runBlocks (a, 110.0f, 0.25f, 256, { 256 }, 48000);
        const auto big = runBlocks (b, 110.0f, 0.25f, 256, { 4096, 37, 1000, 256, 3 }, 48000);
        double err = 0;
        for (size_t i = 0; i < ref.size(); ++i) err = std::max (err, (double) std::abs (ref[i] - big[i]));
        std::printf ("  diferença máxima: %.2e\n", err);
        check (err < 1.0e-5, "mesmo resultado com qualquer tamanho de bloco");
        check (finite (big), "sem NaN/Inf");
    }

    std::printf ("Bypass da DAW mantém o alinhamento (atraso = latência)\n");
    {
        AuraBassAudioProcessor p;
        p.prepareToPlay (sr, blockSize);
        const int lat = p.getLatencySamples();
        juce::AudioBuffer<float> buf (2, blockSize); juce::MidiBuffer midi;
        std::vector<float> in, out;
        for (int blk = 0; blk < 8; ++blk)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const float x = 0.3f * (float) std::sin (0.013 * (blk * blockSize + i));
                buf.setSample (0, i, x); buf.setSample (1, i, x); in.push_back (x);
            }
            p.processBlockBypassed (buf, midi);
            for (int i = 0; i < blockSize; ++i) out.push_back (buf.getSample (0, i));
        }
        double err = 0;
        for (size_t i = (size_t) lat; i < out.size(); ++i) err = std::max (err, (double) std::abs (out[i] - in[i - (size_t) lat]));
        std::printf ("  latência %d amostras, erro %.2e\n", lat, err);
        check (lat > 0 && err < 1.0e-6, "bypass atrasa exatamente a latência");
    }

    std::printf ("Cauda informada à DAW\n");
    {
        AuraBassAudioProcessor p;
        std::printf ("  tail = %.3f s\n", p.getTailLengthSeconds());
        check (p.getTailLengthSeconds() > 0.02, "tail cobre a IR do CAB");
    }

    std::printf ("Silêncio na entrada = silêncio na saída\n");
    {
        AuraBassAudioProcessor p;
        const auto out = runBlocks (p, 100.0f, 0.0f, blockSize, { blockSize }, 48000);
        double peak = 0; for (auto x : out) peak = std::max (peak, (double) std::abs (x));
        std::printf ("  pico na saída: %.2e\n", peak);
        check (peak < 1.0e-6, "sem ruído/DC com entrada em silêncio");
    }

    std::printf ("Outras taxas de amostragem\n");
    for (double rate : { 44100.0, 96000.0, 192000.0 })
    {
        AuraBassAudioProcessor p;
        p.prepareToPlay (rate, 512);
        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
        bool ok = true;
        for (int blk = 0; blk < 200; ++blk)
        {
            for (int i = 0; i < 512; ++i) { const float x = 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 100.0 * (blk * 512 + i) / rate); buf.setSample (0, i, x); buf.setSample (1, i, x); }
            p.processBlock (buf, midi);
            for (int i = 0; i < 512; ++i) ok = ok && std::isfinite (buf.getSample (0, i)) && std::abs (buf.getSample (0, i)) < 4.0f;
        }
        std::printf ("  %.0f Hz: latência %d\n", rate, p.getLatencySamples());
        check (ok, "processa sem NaN/Inf nem explosão");
    }

    std::printf ("Entrada mono, saída estéreo\n");
    {
        AuraBassAudioProcessor p;
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (juce::AudioChannelSet::mono());
        l.outputBuses.add (juce::AudioChannelSet::stereo());
        check (p.setBusesLayout (l), "layout mono→estéreo aceito");
        std::vector<float> right;
        const auto left = runBlocks (p, 100.0f, 0.25f, blockSize, { blockSize }, 24000, &right);
        double diff = 0, peak = 0;
        for (size_t i = 0; i < left.size(); ++i) { diff = std::max (diff, (double) std::abs (left[i] - right[i])); peak = std::max (peak, (double) std::abs (right[i])); }
        check (diff < 1.0e-7 && peak > 0.05, "os dois lados saem iguais");
    }

    std::printf ("Religar o DRIVE e o CAB depois de um tempo desligados, sem estalo\n");
    {
        AuraBassAudioProcessor p;
        setParam (p, ParamIDs::driveOn, 0.0f);
        setParam (p, ParamIDs::cab, 0.0f);
        p.prepareToPlay (sr, blockSize);
        juce::AudioBuffer<float> buf (2, blockSize); juce::MidiBuffer midi;
        std::vector<float> out;
        for (int blk = 0; blk < 240; ++blk)
        {
            if (blk == 100) setParam (p, ParamIDs::driveOn, 1.0f);
            if (blk == 160) setParam (p, ParamIDs::cab, 1.0f);
            for (int i = 0; i < blockSize; ++i)
            {
                const float x = 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 100.0 * (blk * blockSize + i) / sr);
                buf.setSample (0, i, x); buf.setSample (1, i, x);
            }
            p.processBlock (buf, midi);
            for (int i = 0; i < blockSize; ++i) out.push_back (buf.getSample (0, i));
        }
        const double steady = maxJump (out, 20000, 50000);
        const double driveOn = maxJump (out, 100 * blockSize - 100, 100 * blockSize + 3000);
        const double cabOn = maxJump (out, 160 * blockSize - 100, 160 * blockSize + 3000);
        const double after = maxJump (out, 200 * blockSize, 240 * blockSize);
        std::printf ("  maior salto: estável %.4f, DRIVE on %.4f, CAB on %.4f, depois %.4f\n", steady, driveOn, cabOn, after);
        check (driveOn <= std::max (steady, after) * 1.15, "DRIVE volta sem estalo");
        check (cabOn <= std::max (steady, after) * 1.15, "CAB volta sem estalo");
    }

    std::printf ("EQ girando rápido não gera degraus (zipper)\n");
    {
        AuraBassAudioProcessor p;
        setParam (p, ParamIDs::cab, 0.0f);
        setParam (p, ParamIDs::driveOn, 0.0f);
        p.prepareToPlay (sr, blockSize);
        juce::AudioBuffer<float> buf (2, blockSize); juce::MidiBuffer midi;
        std::vector<float> out;
        for (int blk = 0; blk < 120; ++blk)
        {
            if (blk >= 40 && blk < 80) setParam (p, ParamIDs::low, (blk % 2 == 0) ? 12.0f : -12.0f);   // salta 24 dB a cada bloco
            for (int i = 0; i < blockSize; ++i)
            {
                const float x = 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 60.0 * (blk * blockSize + i) / sr);
                buf.setSample (0, i, x); buf.setSample (1, i, x);
            }
            p.processBlock (buf, midi);
            for (int i = 0; i < blockSize; ++i) out.push_back (buf.getSample (0, i));
        }
        const double steady = maxJump (out, 10000, 20000);
        const double moving = maxJump (out, 40 * blockSize, 80 * blockSize);
        std::printf ("  maior salto: parado %.4f, girando %.4f\n", steady, moving);
        check (moving < steady * 4.0 && finite (out), "variação suave do EQ");
    }

    std::printf ("Preset atual salvo na sessão\n");
    {
        AuraBassAudioProcessor a, b;
        a.setCurrentProgram (2);
        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        check (b.getCurrentProgram() == 2, "preset restaurado");
        check (std::abs (b.apvts.getRawParameterValue (ParamIDs::drive)->load() + 8.0f) < 0.05f, "valores do preset restaurados");
    }

    std::printf ("Afinador: precisão da detecção\n");
    {
        struct Case { const char* name; double hz; double fundamental, second, third; double rate; };
        const Case cases[] = {
            { "B0 (Si grave, 5 cordas)", 30.868, 1.0, 0.0, 0.0, 48000.0 },
            { "E1 (Mi)",                 41.203, 1.0, 0.0, 0.0, 48000.0 },
            { "A1 (Lá)",                 55.0,   1.0, 0.0, 0.0, 44100.0 },
            { "D2 (Ré)",                 73.416, 1.0, 0.0, 0.0, 96000.0 },
            { "G2 (Sol)",                97.999, 1.0, 0.0, 0.0, 48000.0 },
            { "E1 com harmônicos fortes", 41.203, 0.3, 1.0, 0.6, 48000.0 },
            { "A1 +12 cents",            55.0 * std::pow (2.0, 12.0 / 1200.0), 0.5, 0.8, 0.4, 48000.0 },
            { "G4 (casa 24)",            391.995, 1.0, 0.3, 0.0, 48000.0 },
        };
        for (const auto& c : cases)
        {
            Tuner t;
            t.prepare (c.rate);
            std::vector<float> block (480);
            const float* chans[1] = { block.data() };
            Tuner::Reading r;
            for (int blk = 0; blk < 60; ++blk)
            {
                for (int i = 0; i < 480; ++i)
                {
                    const double ph = 2.0 * juce::MathConstants<double>::pi * c.hz * (blk * 480 + i) / c.rate;
                    block[(size_t) i] = (float) (0.2 * (c.fundamental * std::sin (ph) + c.second * std::sin (2 * ph + 0.7) + c.third * std::sin (3 * ph + 1.9)));
                }
                t.push (chans, 1, 480);
                r = t.analyse();
            }
            const double errCents = r.valid ? 1200.0 * std::log2 ((double) r.hz / c.hz) : 999.0;
            std::printf ("  %-26s %8.3f Hz -> %8.3f Hz  %s%d  erro %+.2f cents\n", c.name, c.hz, (double) r.hz,
                         Tuner::noteName (r.midiNote).toRawUTF8(), Tuner::noteOctave (r.midiNote), errCents);
            check (r.valid && std::abs (errCents) < 1.0, "erro menor que 1 cent");
        }

        Tuner t;
        t.prepare (48000.0);
        std::vector<float> block (480);
        const float* chans[1] = { block.data() };
        juce::Random rnd (3);
        Tuner::Reading r;
        for (int blk = 0; blk < 60; ++blk)
        {
            for (auto& x : block) x = 0.2f * (rnd.nextFloat() * 2.0f - 1.0f);
            t.push (chans, 1, 480);
            r = t.analyse();
        }
        check (! r.valid, "ruído não vira nota");
        for (int blk = 0; blk < 60; ++blk)
        {
            std::fill (block.begin(), block.end(), 0.0f);
            t.push (chans, 1, 480);
            r = t.analyse();
        }
        check (! r.valid, "silêncio não vira nota");
    }

    std::printf ("Afinador ligado: saída muda, detector recebe a entrada\n");
    {
        AuraBassAudioProcessor p;
        setParam (p, ParamIDs::tuner, 1.0f);
        const auto out = runBlocks (p, 41.203f, 0.25f, blockSize, { blockSize }, 48000);
        double peak = 0; for (size_t i = 4800; i < out.size(); ++i) peak = std::max (peak, (double) std::abs (out[i]));
        const auto r = p.getTuner().analyse();
        std::printf ("  pico na saída %.2e, nota %s%d (%.2f Hz)\n", peak, Tuner::noteName (r.midiNote).toRawUTF8(), Tuner::noteOctave (r.midiNote), (double) r.hz);
        check (peak < 1.0e-6, "saída muda");
        check (r.valid && r.midiNote == 28 && std::abs (r.cents) < 1.0f, "detecta E1 pela entrada do plugin");

        juce::MemoryBlock mb;
        p.getStateInformation (mb);
        AuraBassAudioProcessor q;
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        check (q.apvts.getRawParameterValue (ParamIDs::tuner)->load() < 0.5f, "sessão nunca reabre com o afinador ligado");
    }

    std::printf ("Ligar/desligar o afinador sem estalo\n");
    {
        AuraBassAudioProcessor p;
        p.prepareToPlay (sr, blockSize);
        juce::AudioBuffer<float> buf (2, blockSize); juce::MidiBuffer midi;
        std::vector<float> out;
        for (int blk = 0; blk < 150; ++blk)
        {
            if (blk == 60) setParam (p, ParamIDs::tuner, 1.0f);
            if (blk == 100) setParam (p, ParamIDs::tuner, 0.0f);
            for (int i = 0; i < blockSize; ++i)
            {
                const float x = 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 100.0 * (blk * blockSize + i) / sr);
                buf.setSample (0, i, x); buf.setSample (1, i, x);
            }
            p.processBlock (buf, midi);
            for (int i = 0; i < blockSize; ++i) out.push_back (buf.getSample (0, i));
        }
        const double steady = maxJump (out, 10000, 30000);
        const double sw = std::max (maxJump (out, 60 * blockSize - 50, 60 * blockSize + 3000), maxJump (out, 100 * blockSize - 50, 100 * blockSize + 3000));
        std::printf ("  maior salto: estável %.4f, nas trocas %.4f\n", steady, sw);
        check (sw <= steady * 1.15, "fade sem descontinuidade");
    }

    // Salva uma imagem da interface (útil para conferir o layout sem abrir um host)
    if (auto* outPath = std::getenv ("AURA_SNAPSHOT"))
    {
        AuraBassAudioProcessor p;
        if (auto* hzText = std::getenv ("AURA_SNAPSHOT_TUNER"))
        {
            // Afinador ligado, com uma nota tocando (frequência em Hz)
            setParam (p, ParamIDs::tuner, 1.0f);
            runBlocks (p, juce::String (hzText).getFloatValue(), 0.25f, blockSize, { blockSize }, 48000);
        }
        if (std::getenv ("AURA_SNAPSHOT_OFF") != nullptr)
        {
            setParam (p, ParamIDs::driveOn, 0.0f);
            setParam (p, ParamIDs::cab, 0.0f);
            setParam (p, ParamIDs::drive, 9.5f);
            setParam (p, ParamIDs::low, -12.0f);
        }
        if (auto* sc = std::getenv ("AURA_SNAPSHOT_SCALE"))
            p.setUiScale (juce::String (sc).getFloatValue());

        // Tela de ativação: força o teste a parecer vencido só para a imagem
        if (std::getenv ("AURA_SNAPSHOT_LICENCA") != nullptr)
        {
            p.licenca.agora = [] { return (double) juce::Time::currentTimeMillis()
                                          + 400.0 * AuraLicenca::umDiaMs; };
            p.revisarLicenca();
        }

        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        std::printf ("Editor %dx%d\n", ed->getWidth(), ed->getHeight());
        if (std::getenv ("AURA_SNAPSHOT_TUNER") != nullptr
            || std::getenv ("AURA_SNAPSHOT_LICENCA") != nullptr)
            for (int i = 0; i < 40; ++i)     // deixa o timer da interface rodar
                juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::File file (outPath);
        file.deleteFile();
        juce::FileOutputStream os (file);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("Snapshot salvo em %s\n", outPath);
    }

    std::printf (failures == 0 ? "\nTodos os testes passaram.\n" : "\n%d teste(s) falharam.\n", failures);
    return failures == 0 ? 0 : 1;
}
