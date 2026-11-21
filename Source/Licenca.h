// AURA Bass Preamp — licença: teste de 15 dias e ativação por chave
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
// cópia, redistribuição ou obra derivada apenas com autorização por escrito.
// Licença: ver LICENSE  |  https://github.com/ledixplugins/Drive
//
// As regras são as mesmas do AURA Synth Bass:
//   - teste de 15 dias contados da primeira abertura;
//   - o estado é guardado em dois lugares, então reinstalar não reinicia o teste;
//   - adiantar o relógio não estende o teste; atrasar bloqueia;
//   - depois do teste, chave de ativação (o limite de aparelhos é do servidor);
//   - uma vez ativada, funciona até 30 dias sem internet antes de revalidar.
//
// A lógica pura (avaliar, mesclar, dias restantes) fica em funções livres, para
// poder ser testada sem tocar em arquivo nem em rede.

#pragma once

#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>

namespace AuraLicenca
{
    inline constexpr int    diasDeTeste     = 15;
    inline constexpr int    diasDeTolerancia = 30;   // uso offline depois de ativada
    inline constexpr double umDiaMs         = 24.0 * 60.0 * 60.0 * 1000.0;
    inline constexpr const char* servidor   = "https://api.lemonsqueezy.com/v1/licenses";
    inline constexpr const char* semente    = "AURA-BASS-PREAMP-TRG-2026";

    /** O que fica guardado no disco. Tempos em milissegundos desde 1970. */
    struct Estado
    {
        double t0   = 0.0;    // primeira abertura
        double seen = 0.0;    // maior data já vista (pega o relógio atrasado)
        double val  = 0.0;    // última validação com o servidor
        juce::String chave, instancia, aparelho;

        bool temChave() const noexcept { return chave.isNotEmpty() && instancia.isNotEmpty(); }
    };

    enum class Status
    {
        teste,        // dentro dos 15 dias
        expirado,     // teste terminou, sem chave
        relogio,      // o relógio do sistema voltou no tempo
        licenciada,   // chave ativa e validada
        revalidar     // chave ativa, mas passou da tolerância offline
    };

    //==============================================================================
    // Lógica pura — sem disco, sem rede, sem relógio escondido.

    /** Junta os dois estados guardados: vale a primeira abertura mais antiga, a
        maior data vista e a validação mais recente. */
    Estado mesclar (const Estado& a, const Estado& b);

    Status avaliar (const Estado& e, double agora);

    /** Dias que faltam do teste (0 quando acabou). */
    int diasRestantes (const Estado& e, double agora);

    /** O plugin processa som? Teste válido ou licença em dia. */
    inline bool liberado (Status s) noexcept
    {
        return s == Status::teste || s == Status::licenciada;
    }

    juce::String textoDoStatus (Status s, int diasRestantes);

    juce::var paraVar (const Estado& e);
    Estado deVar (const juce::var& v);

    //==============================================================================
    /** Cuida do disco, do relógio e do servidor. Uma instância por plugin. */
    class Gerente
    {
    public:
        Gerente();

        Status status() const noexcept           { return statusAtual; }
        int diasRestantes() const noexcept       { return diasQueFaltam; }
        bool liberado() const noexcept           { return AuraLicenca::liberado (statusAtual); }
        juce::String idDoAparelho() const        { return idAparelho; }
        juce::String nomeDoAparelho() const      { return nomeAparelho; }
        juce::String chaveMascarada() const;

        /** Relê o disco, marca o uso de hoje e recalcula o status. */
        void atualizar();

        /** Só recalcula contra o relógio, sem tocar no disco (barato). */
        void recalcular();

        struct Resposta { bool ok = false; juce::String erro; };

        /** Chamadas de rede — bloqueiam; nunca use na thread de áudio. */
        Resposta ativar (const juce::String& chave);
        Resposta desativar();
        Resposta revalidar();

        // Visível para os testes: caminhos e relógio podem ser trocados.
        std::function<double()> agora { [] { return (double) juce::Time::currentTimeMillis(); } };
        juce::File arquivo1, arquivo2;

    private:
        void carregar();
        void gravar();
        juce::var chamar (const juce::String& caminho, const juce::StringPairArray& campos, int& httpStatus);

        Estado estado;
        Status statusAtual = Status::teste;
        int diasQueFaltam = diasDeTeste;
        juce::String idAparelho, nomeAparelho;
    };
}
