// AURA Bass Preamp — interface do plugin
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
// cópia, redistribuição ou obra derivada apenas com autorização por escrito.
// Licença: ver LICENSE  |  https://github.com/ledixplugins/Drive
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Layout.h"

//==============================================================================
// Imagens da interface (Resources/ui) e fonte dos valores, compartilhadas entre instâncias
struct AuraAssets
{
    AuraAssets();

    juce::Image background, knobBody, pointer, ledDriveOn, ledCabOn, cabOff, footswitchDown, arcsLit, ledTunerOn, tunerDown;
    juce::Typeface::Ptr valueTypeface, titleTypeface;
    juce::Font valueFont { juce::FontOptions() };
};

//==============================================================================
// Knob: a arte já está no fundo; aqui só o indicador gira.
// Shift = ajuste fino; clique direito = menu da DAW para o parâmetro (automação etc.).
class PointerKnob : public juce::Slider
{
public:
    explicit PointerKnob (const AuraAssets&);
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    juce::Point<float> centre;                 // centro exato do knob, em coordenadas locais
    std::function<void()> onContextMenu;

    // Os knobs da faixa de efeitos não têm arte no fundo: desenham o próprio
    // corpo, a partir do mesmo sprite recortado da placa.
    float raioCorpo = 0.0f;

private:
    const AuraAssets& assets;
    bool popupClick = false;
};

//==============================================================================
// Área clicável invisível (footswitch do DRIVE e chave do CAB). Só o clique esquerdo alterna;
// o clique direito abre o menu (da DAW para o parâmetro).
class HotSpot : public juce::ToggleButton
{
public:
    HotSpot() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics&, bool, bool) override {}
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onContextMenu;

private:
    bool popupClick = false;
};

//==============================================================================
// Tela de ativação: aparece quando o teste acaba, quando a licença precisa de
// revalidação ou quando o relógio do sistema está atrasado.
class LicencaOverlay : public juce::Component,
                       private juce::Timer
{
public:
    explicit LicencaOverlay (AuraBassPreampAudioProcessor&);
    ~LicencaOverlay() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Mostra ou esconde conforme o status, e atualiza os textos. */
    void atualizar();

    /** Abre a tela mesmo durante o teste (item do menu). */
    void abrirSempre();

private:
    void timerCallback() override;
    void tentarAtivar();
    void tentarDesativar();

    AuraBassPreampAudioProcessor& processor;
    juce::TextEditor campoChave;
    juce::TextButton botaoAcao;
    juce::TextButton botaoComprar;
    juce::String mensagem, erro;
    bool ocupado = false;
    bool forcada = false;
    std::unique_ptr<juce::Thread> trabalho;
};

//==============================================================================
// Medidor de entrada com ajuste de trim: mostra o pico que está chegando e
// acende o ponto de clip. Arrastar na vertical muda o trim; duplo clique zera.
class InputMeter : public juce::Component
{
public:
    InputMeter (AuraBassPreampAudioProcessor&, juce::RangedAudioParameter* trimParam);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    /** Chamado pelo timer do painel: atualiza a balística e repinta se mudou. */
    void tick();

private:
    float trimDb() const;
    void setTrim (float db);

    AuraBassPreampAudioProcessor& processor;
    juce::RangedAudioParameter* trim = nullptr;

    float nivel = -120.0f;     // pico suavizado, em dB
    float retencao = -120.0f;  // marca do pico recente
    int retencaoFrames = 0;
    bool clip = false;
    int clipFrames = 0;

    bool arrastando = false;
    float trimInicial = 0.0f;
    int yInicial = 0;
    int mostrarTrimFrames = 0;
};

//==============================================================================
// Botão de menu: três traços discretos, no tom dos textos do painel
class MenuButton : public juce::Button
{
public:
    MenuButton() : juce::Button ("Menu") { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

//==============================================================================
// Visor do afinador: nota, desvio em cents e frequência. Cobre os knobs enquanto ligado.
class TunerDisplay : public juce::Component
{
public:
    explicit TunerDisplay (const AuraAssets& a) : assets (a) {}

    void restart();                              // ao ligar: descarta leituras antigas
    void update (const Tuner::Reading&);         // chamado pelo timer do painel
    void paint (juce::Graphics&) override;

private:
    const AuraAssets& assets;
    bool hasNote = false;
    int note = 0, candidate = -1000, candidateCount = 0, warmup = 0;
    float cents = 0.0f, hz = 0.0f;
    double lastValid = 0.0;
};

//==============================================================================
// O painel inteiro, sempre no tamanho original (726x339); o editor aplica a escala
class AuraPanel : public juce::Component,
                  private juce::Timer
{
public:
    explicit AuraPanel (AuraBassPreampAudioProcessor&);
    ~AuraPanel() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    std::function<void (float)> onScaleChosen;
    std::function<void (juce::RangedAudioParameter&)> onParameterContextMenu;
    float currentScale = 1.0f;

    static juce::String formatValue (const juce::String& paramId, float value);
    void showMainMenu (bool fromButton);

private:
    void timerCallback() override;
    void showCabMenu();
    void chooseIrFile();
    void repaintKnobArea (size_t index);
    void refreshValue (size_t index);
    void commitValue (size_t index);
    void updateDriveDim();
    juce::Rectangle<float> cabIrArea() const;

    AuraBassPreampAudioProcessor& processor;
    juce::SharedResourcePointer<AuraAssets> assets;

    std::vector<std::unique_ptr<PointerKnob>> knobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> knobAttachments;

    std::vector<std::unique_ptr<juce::Label>> valueLabels;   // valores editáveis (clique para digitar)
    MenuButton menuButton;

    std::unique_ptr<InputMeter> meter;
    std::vector<std::unique_ptr<PointerKnob>> efeitoKnobs;
    std::vector<std::unique_ptr<juce::Label>> efeitoLabels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> efeitoAttachments;
    std::unique_ptr<LicencaOverlay> licencaOverlay;
    int quadrosAteRevisarLicenca = 0;
    HotSpot driveSpot, cabSpot, tunerSpot;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> driveAttachment, cabAttachment, tunerAttachment;
    TunerDisplay tunerDisplay { *assets };
    bool driveOn = true, cabOn = true, tunerOn = false;
    int cabIrVersion = -1;
    juce::String cabIrName;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraPanel)
};

//==============================================================================
class AuraBassPreampAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::ComponentListener
{
public:
    explicit AuraBassPreampAudioProcessorEditor (AuraBassPreampAudioProcessor&);
    ~AuraBassPreampAudioProcessorEditor() override;

    void resized() override;
    void setScale (float scale);
    void parentHierarchyChanged() override;

private:
    void showHostMenu (juce::RangedAudioParameter&);

    // No app autônomo, guarda e devolve a posição da janela entre aberturas.
    void componentMovedOrResized (juce::Component&, bool, bool) override;
    void guardaJanela();
    juce::Component* janela = nullptr;

    AuraBassPreampAudioProcessor& processor;
    AuraPanel panel;
    juce::TooltipWindow tooltips { this, 900 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraBassPreampAudioProcessorEditor)
};
