// AURA Preamp Bass — licença: teste de 15 dias e ativação por chave
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
// cópia, redistribuição ou obra derivada apenas com autorização por escrito.
// Licença: ver LICENSE  |  https://github.com/ledixplugins/Drive

#include "Licenca.h"

namespace AuraLicenca
{
using namespace juce;

//==============================================================================
Estado mesclar (const Estado& a, const Estado& b)
{
    Estado r = a.val >= b.val ? a : b;          // a validação mais recente manda

    // A primeira abertura é a mais antiga das duas que existirem.
    if (a.t0 > 0.0 && b.t0 > 0.0) r.t0 = jmin (a.t0, b.t0);
    else                          r.t0 = jmax (a.t0, b.t0);

    r.seen = jmax (a.seen, b.seen);             // e a maior data já vista

    // O e-mail e o "já usou o teste" vêm de quem tiver e-mail; a validação mais
    // recente (r, acima) já decide isso na prática, mas cobre o caso de um dos
    // dois arquivos ainda não ter sido escrito com o e-mail.
    if (r.email.isEmpty())
    {
        r.email = a.email.isNotEmpty() ? a.email : b.email;
        r.reutilizado = a.email.isNotEmpty() ? a.reutilizado : b.reutilizado;
    }

    return r;
}

Status avaliar (const Estado& e, double agora)
{
    // Relógio atrasado mais de um dia em relação ao que já vimos: bloqueia.
    if (e.seen > 0.0 && agora < e.seen - umDiaMs)
        return Status::relogio;

    if (e.temChave())
        return (agora - e.val) <= diasDeTolerancia * umDiaMs ? Status::licenciada
                                                             : Status::revalidar;

    // O teste só começa depois que a pessoa informa o e-mail.
    if (! e.temEmail())
        return Status::precisaEmail;

    return diasRestantes (e, agora) > 0 ? Status::teste : Status::expirado;
}

int diasRestantes (const Estado& e, double agora)
{
    if (e.t0 <= 0.0) return diasDeTeste;

    // Conta pelo maior entre o relógio de agora e a maior data já vista, para
    // que atrasar o relógio não devolva dias de teste.
    const double referencia = jmax (agora, e.seen);
    const int passados = (int) std::floor ((referencia - e.t0) / umDiaMs);
    return jlimit (0, diasDeTeste, diasDeTeste - passados);
}

String textoDoStatus (Status s, int dias)
{
    switch (s)
    {
        case Status::precisaEmail: return String::fromUTF8 ("Informe seu e-mail para iniciar o teste grátis");
        case Status::teste:      return dias == 1 ? String::fromUTF8 ("Teste: último dia")
                                                  : String::fromUTF8 ("Teste: ") + String (dias) + String::fromUTF8 (" dias restantes");
        case Status::expirado:   return String::fromUTF8 ("O teste de 15 dias terminou");
        case Status::relogio:    return String::fromUTF8 ("O relógio do sistema está atrasado");
        case Status::licenciada: return String::fromUTF8 ("Licença ativa neste aparelho");
        case Status::revalidar:  return String::fromUTF8 ("Precisamos revalidar a sua licença");
    }
    return {};
}

var paraVar (const Estado& e)
{
    auto* o = new DynamicObject();
    o->setProperty ("t0", e.t0);
    o->setProperty ("seen", e.seen);
    o->setProperty ("val", e.val);
    o->setProperty ("key", e.chave);
    o->setProperty ("inst", e.instancia);
    o->setProperty ("name", e.aparelho);
    o->setProperty ("email", e.email);
    o->setProperty ("reused", e.reutilizado);
    return var (o);
}

Estado deVar (const var& v)
{
    Estado e;
    if (auto* o = v.getDynamicObject())
    {
        e.t0        = (double) o->getProperty ("t0");
        e.seen      = (double) o->getProperty ("seen");
        e.val       = (double) o->getProperty ("val");
        e.chave     = o->getProperty ("key").toString();
        e.instancia = o->getProperty ("inst").toString();
        e.aparelho  = o->getProperty ("name").toString();
        e.email     = o->getProperty ("email").toString();
        e.reutilizado = (bool) o->getProperty ("reused");
    }
    return e;
}

//==============================================================================
namespace
{
    File pastaDeDados()
    {
        return File::getSpecialLocation (File::userApplicationDataDirectory)
                   .getChildFile (
                     #if JUCE_MAC
                       "Application Support/AURA Preamp Bass"
                     #else
                       "AURA Preamp Bass"
                     #endif
                   );
    }

    File pastaEscondida()
    {
        auto casa = File::getSpecialLocation (File::userHomeDirectory);
       #if JUCE_WINDOWS
        return casa.getChildFile ("AppData/Local/AuraPreampBass");
       #else
        return casa.getChildFile (".config");
       #endif
    }

    Estado lerArquivo (const File& f)
    {
        if (! f.existsAsFile()) return {};
        return deVar (JSON::parse (f.loadFileAsString()));
    }

    void gravarArquivo (const File& f, const Estado& e)
    {
        f.getParentDirectory().createDirectory();
        f.replaceWithText (JSON::toString (paraVar (e), true));
    }
}

//==============================================================================
Gerente::Gerente()
{
    arquivo1 = pastaDeDados().getChildFile ("lic.json");
    arquivo2 = pastaEscondida().getChildFile (
                 #if JUCE_WINDOWS
                   ".lic"
                 #else
                   ".aura-preamp-bass-lic"
                 #endif
               );

    nomeAparelho = SystemStats::getComputerName();
    if (nomeAparelho.isEmpty()) nomeAparelho = "Meu computador";
    nomeAparelho = nomeAparelho.substring (0, 24);

    const auto cru = String (semente) + "|" + SystemStats::getComputerName()
                   + "|" + SystemStats::getLogonName();
    idAparelho = SHA256 (cru.toUTF8()).toHexString().substring (0, 32);

    atualizar();
}

void Gerente::carregar()
{
    estado = mesclar (lerArquivo (arquivo1), lerArquivo (arquivo2));
}

void Gerente::gravar()
{
    gravarArquivo (arquivo1, estado);
    gravarArquivo (arquivo2, estado);
}

void Gerente::atualizar()
{
    carregar();

    const double agoraMs = agora();
    // t0 só é marcada quando o teste é iniciado com e-mail (iniciarTeste), não aqui.
    if (agoraMs > estado.seen) estado.seen = agoraMs;
    gravar();

    statusAtual   = avaliar (estado, agoraMs);
    diasQueFaltam = AuraLicenca::diasRestantes (estado, agoraMs);
}

void Gerente::recalcular()
{
    const double agoraMs = agora();
    statusAtual   = avaliar (estado, agoraMs);
    diasQueFaltam = AuraLicenca::diasRestantes (estado, agoraMs);
}

String Gerente::chaveMascarada() const
{
    if (estado.chave.isEmpty()) return {};
    return estado.chave.substring (0, 8) + String::fromUTF8 ("••••••••");
}

//==============================================================================
var Gerente::chamar (const String& baseUrl, const String& caminho, const StringPairArray& campos, int& httpStatus)
{
    httpStatus = 0;

    String corpo;
    for (const auto& k : campos.getAllKeys())
    {
        if (corpo.isNotEmpty()) corpo << "&";
        corpo << URL::addEscapeChars (k, true) << "=" << URL::addEscapeChars (campos[k], true);
    }

    URL u = URL (baseUrl + caminho).withPOSTData (corpo);

    StringPairArray cabecalhos;
    auto opcoes = URL::InputStreamOptions (URL::ParameterHandling::inPostData)
                      .withExtraHeaders ("Accept: application/json\r\n"
                                         "Content-Type: application/x-www-form-urlencoded\r\n")
                      .withConnectionTimeoutMs (10000)
                      .withResponseHeaders (&cabecalhos)
                      .withStatusCode (&httpStatus);

    std::unique_ptr<InputStream> entrada (u.createInputStream (opcoes));
    if (entrada == nullptr) return {};

    return JSON::parse (entrada->readEntireStreamAsString());
}

namespace
{
    // Mesma validação simples do site/app: só a forma do endereço, sem confirmar
    // a caixa de entrada de verdade (decisão do Thiago: sem e-mail de confirmação).
    bool pareceEmailValido (const String& email)
    {
        if (email.isEmpty() || email.containsChar (' ') || email.length() > 254) return false;
        const auto arroba = email.indexOfChar ('@');
        if (arroba <= 0 || arroba != email.lastIndexOfChar ('@')) return false;
        const auto dominio = email.substring (arroba + 1);
        const auto ponto = dominio.lastIndexOfChar ('.');
        return ponto > 0 && dominio.substring (ponto + 1).length() >= 2;
    }
}

Gerente::Resposta Gerente::iniciarTeste (const String& emailDigitado)
{
    const auto email = emailDigitado.trim().toLowerCase();
    if (! pareceEmailValido (email))
        return { false, String::fromUTF8 ("Digite um e-mail válido.") };

    StringPairArray campos;
    campos.set ("email", email);
    campos.set ("device", idAparelho);
    campos.set ("product", produto);

    int http = 0;
    const auto r = chamar (servidorTeste, "/trial", campos, http);

    if (http == 0)
        return { false, String::fromUTF8 ("Sem conexão com o servidor. O primeiro acesso precisa de internet para registrar o teste.") };

    if (auto* o = r.getDynamicObject())
    {
        if ((bool) o->getProperty ("ok") && o->hasProperty ("started_at"))
        {
            const double iniciadoEm = (double) o->getProperty ("started_at");
            estado.email = email;
            estado.t0 = estado.t0 > 0.0 ? jmin (estado.t0, iniciadoEm) : iniciadoEm;
            estado.reutilizado = ! (bool) o->getProperty ("new");
            const double agoraMs = agora();
            if (agoraMs > estado.seen) estado.seen = agoraMs;
            gravar();
            atualizar();
            return { true, {} };
        }

        const auto erro = o->getProperty ("error").toString();
        if (erro.isNotEmpty()) return { false, erro };
    }

    return { false, String::fromUTF8 ("Não foi possível iniciar o teste agora. Tente de novo.") };
}

Gerente::Resposta Gerente::ativar (const String& chave)
{
    const auto limpa = chave.trim();
    if (limpa.isEmpty())
        return { false, String::fromUTF8 ("Cole a chave de ativação.") };

    StringPairArray campos;
    campos.set ("license_key", limpa);
    campos.set ("instance_name", nomeAparelho + " (" + idAparelho.substring (0, 8) + ")");

    int http = 0;
    const auto r = chamar (servidor, "/activate", campos, http);

    if (http == 0)
        return { false, String::fromUTF8 ("Sem internet. Conecte-se e tente de novo.") };

    if (auto* o = r.getDynamicObject())
    {
        if ((bool) o->getProperty ("activated"))
        {
            estado.chave = limpa;
            const auto inst = o->getProperty ("instance");
            estado.instancia = inst.getDynamicObject() != nullptr
                                   ? inst.getDynamicObject()->getProperty ("id").toString()
                                   : String();
            estado.aparelho = nomeAparelho;
            estado.val = agora();
            gravar();
            atualizar();
            return { true, {} };
        }

        const auto erro = o->getProperty ("error").toString();
        if (erro.isNotEmpty()) return { false, erro };
    }

    return { false, String::fromUTF8 ("Chave não aceita. Confira se copiou inteira.") };
}

Gerente::Resposta Gerente::desativar()
{
    if (! estado.temChave())
        return { false, String::fromUTF8 ("Não há licença ativa neste aparelho.") };

    StringPairArray campos;
    campos.set ("license_key", estado.chave);
    campos.set ("instance_id", estado.instancia);

    int http = 0;
    const auto r = chamar (servidor, "/deactivate", campos, http);

    const bool desativou = http == 404      // a instância já não existe no servidor
                        || (r.getDynamicObject() != nullptr
                            && (bool) r.getDynamicObject()->getProperty ("deactivated"));

    if (! desativou)
        return { false, http == 0 ? String::fromUTF8 ("Sem internet. Conecte-se e tente de novo.")
                                  : String::fromUTF8 ("Não foi possível desativar agora.") };

    estado.chave = {};
    estado.instancia = {};
    estado.val = 0.0;
    gravar();
    atualizar();
    return { true, {} };
}

Gerente::Resposta Gerente::revalidar()
{
    if (! estado.temChave()) return { false, {} };

    StringPairArray campos;
    campos.set ("license_key", estado.chave);
    campos.set ("instance_id", estado.instancia);

    int http = 0;
    const auto r = chamar (servidor, "/validate", campos, http);
    if (http == 0) return { false, String::fromUTF8 ("Sem internet.") };

    if (auto* o = r.getDynamicObject())
    {
        if ((bool) o->getProperty ("valid"))
        {
            estado.val = agora();
            gravar();
            atualizar();
            return { true, {} };
        }
    }

    // O servidor disse que não vale mais: volta ao estado sem chave.
    estado.chave = {};
    estado.instancia = {};
    estado.val = 0.0;
    gravar();
    atualizar();
    return { false, String::fromUTF8 ("Esta chave não está mais ativa neste aparelho.") };
}

} // namespace AuraLicenca
