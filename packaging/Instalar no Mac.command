#!/bin/bash
# AURA Preamp Bass — instala os plugins já compilados neste Mac.
# Rode no Terminal:  bash  (espaço)  e arraste este arquivo para a janela, depois Enter.

set -e
cd "$(dirname "$0")"

for item in "AURA Preamp Bass.vst3" "AURA Preamp Bass.component" "AURA Preamp Bass.app"; do
    if [ ! -e "$item" ]; then
        echo "Não encontrei \"$item\" nesta pasta. Descompacte o AuraPreampBass-Mac.zip inteiro e rode de novo."
        exit 1
    fi
done

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
APP_DIR="$HOME/Applications"
mkdir -p "$VST3_DIR" "$AU_DIR" "$APP_DIR"

echo "Instalando AURA Preamp Bass..."
# Remove a versão antiga (Grit DI), que usa os mesmos códigos de plugin
rm -rf "$VST3_DIR/Grit DI.vst3" "$AU_DIR/Grit DI.component" "$APP_DIR/Grit DI.app"
rm -rf "$VST3_DIR/AURA Preamp Bass.vst3" "$AU_DIR/AURA Preamp Bass.component" "$APP_DIR/AURA Preamp Bass.app"
cp -R "AURA Preamp Bass.vst3" "$VST3_DIR/"
cp -R "AURA Preamp Bass.component" "$AU_DIR/"
cp -R "AURA Preamp Bass.app" "$APP_DIR/"

# Remove o bloqueio de "baixado da internet" e reaplica a assinatura local
for item in "$VST3_DIR/AURA Preamp Bass.vst3" "$AU_DIR/AURA Preamp Bass.component" "$APP_DIR/AURA Preamp Bass.app"; do
    xattr -cr "$item" 2>/dev/null || true
    codesign --force --deep --sign - "$item" >/dev/null 2>&1 || true
done

# Faz o macOS reler a lista de Audio Units (sem isso o Logic pode mostrar a versão antiga)
rm -rf "$HOME/Library/Caches/AudioUnitCache" 2>/dev/null || true
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

echo ""
echo "Validando o Audio Unit..."
if auval -v aufx Grdi Thgo >/tmp/aurapreampbass-auval.log 2>&1; then
    echo "AU validado com sucesso."
else
    echo "Aviso: o auval não passou (detalhes em /tmp/aurapreampbass-auval.log). VST3 e Standalone funcionam mesmo assim."
fi

VERSION=$(defaults read "$AU_DIR/AURA Preamp Bass.component/Contents/Info.plist" CFBundleShortVersionString 2>/dev/null || echo "?")
echo ""
echo "Pronto! AURA Preamp Bass $VERSION instalado."
echo "  VST3:        $VST3_DIR/AURA Preamp Bass.vst3"
echo "  AU:          $AU_DIR/AURA Preamp Bass.component"
echo "  Standalone:  $APP_DIR/AURA Preamp Bass.app"
echo ""
echo "Abra sua DAW e faça uma nova busca de plugins (no Logic: Configurações > Plug-in Manager)."
