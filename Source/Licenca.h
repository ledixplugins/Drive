// AURA Preamp Bass — licença: teste de 15 dias e ativação por chave
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
// cópia, redistribuição ou obra derivada apenas com autorização por escrito.
// Licença: ver LICENSE  |  https://github.com/ledixplugins/Drive
//
// As regras são as mesmas do AURA Synth Bass:
//   - o teste de 15 dias só começa quando a pessoa informa um e-mail (registrado
//     no mesmo servidor do Synth Bass, separado por produto); um e-mail só
//     inicia o teste uma vez;
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
    // Mesmo Worker/banco do AURA Synth Bass (aura-trial), numa rota própria: o
    // produto fica fixo no código do servidor, não é algo que o app possa escolher.
    inline constexpr const char* servidorTeste = "https://aura-trial.ledixplugins.workers.dev";
    inline constexpr const char* caminhoTeste  = "/trial/preamp-bass";
    inline constexpr const char* semente    = "AURA-PREAMP-BASS-TRG-2026";

    /** O que fica guardado no disco. Tempos em milissegundos desde 1970. */
    struct Estado
    {
        double t0   = 0.0;    // primeira abertura (só é marcada quando o teste é iniciado com e-mail)
        double seen = 0.0;    // maior data já vista (pega o relógio atrasado)
        double val  = 0.0;    // última validação com o servidor
        juce::String chave, instancia, aparelho;
        juce::String email;   // e-mail que iniciou o teste
        bool reutilizado = false;  // esse e-mail já tinha usado o teste antes (noutro aparelho)
        juce::String dono;    // prova (owner_token) do acesso total do desenvolvedor, se houver
        double donoVal = 0.0; // última confirmação do acesso total com o servidor

        bool temChave() const noexcept { return chave.isNotEmpty() && instancia.isNotEmpty(); }
        bool temEmail() const noexcept { return email.isNotEmpty(); }
        bool temDono() const noexcept  { return dono.isNotEmpty(); }
    };

    enum class Status
    {
        precisaEmail, // ainda não informou o e-mail: o teste não começou
        teste,        // dentro dos 15 dias
        expirado,     // teste terminou, sem chave
        relogio,      // o relógio do sistema voltou no tempo
        licenciada,   // chave ativa e validada
        acessoTotal,  // acesso total do desenvolvedor (e-mail + código secreto), confirmado com o servidor
        revalidar     // chave (ou acesso do dono) ativa, mas passou da tolerância offline
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
        return s == Status::teste || s == Status::licenciada || s == Status::acessoTotal;
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
        bool estadoReutilizado() const noexcept  { return estado.reutilizado; }
        bool estaComoDono() const noexcept       { return estado.temDono(); }

        /** Relê o disco, marca o uso de hoje e recalcula o status. */
        void atualizar();

        /** Só recalcula contra o relógio, sem tocar no disco (barato). */
        void recalcular();

        struct Resposta { bool ok = false; juce::String erro; };

        /** Chamadas de rede — bloqueiam; nunca use na thread de áudio. */
        Resposta iniciarTeste (const juce::String& email);
        Resposta ativar (const juce::String& chave);
        Resposta desativar();
        Resposta revalidar();

        /** Acesso total do desenvolvedor: e-mail cadastrado no servidor (OWNER_EMAILS)
            mais o código secreto. Guarda a prova assinada que o servidor devolve, para
            revalidar sem precisar digitar o código de novo. */
        Resposta entrarComoDono (const juce::String& email, const juce::String& codigo);

        /** Sai do acesso total neste aparelho (volta ao teste/chave normal). */
        Resposta sairAcessoTotal();

        // Visível para os testes: caminhos e relógio podem ser trocados.
        std::function<double()> agora { [] { return (double) juce::Time::currentTimeMillis(); } };
        juce::File arquivo1, arquivo2;

    private:
        void carregar();
        void gravar();
        juce::var chamar (const juce::String& baseUrl, const juce::String& caminho,
                          const juce::StringPairArray& campos, int& httpStatus);

        Estado estado;
        Status statusAtual = Status::teste;
        int diasQueFaltam = diasDeTeste;
        juce::String idAparelho, nomeAparelho;
    };
}
