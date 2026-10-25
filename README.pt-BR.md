# BOTWRecompAndroid

[English](README.md) | [Français](README.fr.md) | **Português**

> [!NOTE]
> **Objetivo deste fork**
>
> Este fork de [mamcjr/BOTWRecompAndroid](https://github.com/mamcjr/BOTWRecompAndroid) facilita o download das versões
> Android e amplia os idiomas disponíveis nos menus do aplicativo. As adições atuais são:
>
> - **Releases automáticas de APK:** cada push bem-sucedido para `main` gera um APK Android assinado e o publica como
>   pré-release na [página Releases deste fork](https://github.com/tib-kpl/BOTWRecompAndroid/releases).
> - **Menus em francês:** traduções para a tela inicial, configurações, controles, diálogos e atalhos Android,
>   junto dos menus existentes em inglês e português. A interface segue o idioma do aplicativo no Android.
> - **Suporte a futuras traduções:** catálogos separados, recursos de texto Android e verificações na CI facilitam
>   a inclusão de outros idiomas. Veja o [guia de tradução](docs/TRANSLATIONS.md).
>
> O motor de recompilação e o suporte existente ao jogo vêm dos projetos de origem creditados abaixo. As traduções
> dos menus descritas aqui são da interface do aplicativo; o idioma dos textos do jogo tem sua própria configuração.

Uma recompilação estática de *The Legend of Zelda: Breath of the Wild* (Nintendo Switch, versões **1.0.0, 1.6.0 e
1.8.2**) para Android. **Não é emulação**: o código ARM64 do jogo vira C compilado para o seu celular, e os gráficos
(NVN) são traduzidos para Vulkan.

O projeto parte do motor do **[LAS Recomp](https://github.com/peiikai/LASRecomp)**, de peiikai (uma recompilação de
*Link's Awakening*), e do seu porte para Android, o **[LASRecompAndroid](https://github.com/mamcjr/LASRecompAndroid)**.

> [!WARNING]
> **Beta.** Testado num celular só (POCO F7 Ultra, Adreno 830, driver Mesa Turnip). Espere bugs. O plano e o estado
> de cada fase estão em [docs/BOTW-PORT.md](docs/BOTW-PORT.md).

## Plataforma suportada

Android é o único alvo do aplicativo. Os ports e pacotes desktop foram descontinuados em 2026-10-22.
Windows, Linux e macOS podem continuar sendo usados como ambientes de desenvolvimento com o Android SDK/NDK.

## Estado

- **Jogável** da tela de título até o mundo, com saves, no POCO F7 Ultra: 1.0.0 e 1.6.0 a 30 fps, 1.8.2 a 30 ou até
  60 (cerca de 40-47 fps no mundo aberto com um driver Turnip).
- **1.8.2** tem português do Brasil, widescreen (Vídeo > Proporção da tela "Livre": a visão alarga, o HUD vai para as
  bordas da tela) e o ritmo de quadros do SavageWindRecomp.
- **Vídeo:** resolução interna, upscaling AMD FSR 1 / Snapdragon GSR 1, resolução de saída, SMAA.
- **App:** configuração dos arquivos do jogo pelo celular (cada versão na sua pasta, trocadas na página Arquivos do
  Jogo), controles de toque, controles físicos, mods, menus em inglês, francês e português, escolha do driver Vulkan (Turnip
  pelo adrenotools) e opções de compatibilidade do driver.
- **Lacunas conhecidas:** configurar o jogo pelo celular compila o código do jogo nele, e isso ainda não foi medido nem
  testado com o BOTW (espere uma demora longa e o celular quente); GPUs que não são Adreno 8xx usam caminhos mais
  lentos e seguros; as miniaturas dos saves feitos no app ficam pretas; o áudio ainda usa um mixer simplificado.

> [!IMPORTANT]
> Como o LAS Recomp, este projeto é feito quase todo com IA (Claude, da Anthropic, pelo Claude Code), dirigido,
> testado e revisado por um humano.

> [!NOTE]
> **Nenhum arquivo do jogo vem incluído.** Você precisa do seu próprio dump, obtido legalmente, de *Breath of the
> Wild* 1.0.0, 1.6.0 ou 1.8.2 (`exefs` e `romfs`, extraídos). Não serão dadas instruções de como obtê-lo.

## Compatibilidade dos controles

**Controles integrados — AYN Thor, Odin 2 e Retroid:** os botões padrão, os dois analógicos e os gatilhos devem
funcionar pelo suporte a gamepads Android do SDL3, desde que o aparelho seja compatível com o APK. Esses portáteis
**ainda não foram testados com este fork**. Abra **Configurações → Controles** para verificar a detecção e remapear
os botões. A disposição padrão é a da Nintendo: **A à direita, B embaixo**.

## Instalando (beta)

1. Baixe o APK em [Releases](https://github.com/tib-kpl/BOTWRecompAndroid/releases) e instale (Android 11+, arm64,
   Vulkan; cerca de 30 GB livres por versão do jogo).
2. Para guardar os dados do jogo no cartão SD, abra primeiro **Configurações → Arquivos → Armazenamento dos dados
   do jogo** e selecione o cartão. A escolha é mantida nos próximos inícios; mudar o armazenamento reinicia o app
   e não move os arquivos existentes.
3. Vá em **Configurar arquivos do jogo** e escolha a pasta com o seu `exefs` e `romfs`. A versão é reconhecida
   pelos arquivos. O app copia o que precisa e compila o código do jogo no celular.
4. Em celulares com Adreno, um driver Mesa Turnip (Configurações > Arquivos > Driver Vulkan) roda o jogo bem mais
   rápido que o driver do sistema. Se o app parar ao abrir com "Vulkan driver is too old", toque e segure o ícone do
   app e escolha **Escolher driver** para selecionar um antes de o jogo começar.

## Testando no Android (desenvolvedores)

Com o Android SDK / NDK, um celular no adb e o seu dump em `Dump/`:

```bash
cd android/botwrecompandroid && ./gradlew assembleDebug    # o app
android/build_gamelib.sh Dump/01007EF00011E000/exefs  # o código do jogo (cerca de uma hora)
android/push_game.sh Dump/01007EF00011E000           # arquivos e código do jogo para o celular
android/run_boot.sh 120                               # abre o jogo, log em android/out/boot.log
```

## Créditos

- [peiikai](https://github.com/peiikai), pelo [LAS Recomp](https://github.com/peiikai/LASRecomp), o recompilador e o
  runtime de onde este projeto parte (MIT, ver [LICENSE](LICENSE)). O README original fica em [docs/las/](docs/las/).
- [LASRecompAndroid](https://github.com/mamcjr/LASRecompAndroid), o porte dele para Android: o app, a compilação do
  código do jogo no celular, os controles de toque, os mods, a compatibilidade do driver e os menus em português.
- [Revivedx](https://github.com/Revivedx), pelo [SavageWindRecomp](https://github.com/Revivedx/SavageWindRecomp) (MIT),
  a referência para a 1.8.2: ritmo de quadros, resolução interna, as correções do kernel do SDK 20, setjmp.
- [Nycolazs](https://github.com/Nycolazs), pelo LASRecomp-Android-Plus (MIT): compatibilidade de drivers Vulkan.
- A decompilação [zeldaret/botw](https://github.com/zeldaret/botw) e o [open-ead/sead](https://github.com/open-ead/sead),
  pelo que se sabe do código do jogo.
- O launcher do Dusklight, o estilo da tela inicial.
- Upscaling e anti-aliasing: [AMD FidelityFX FSR 1](https://github.com/GPUOpen-Effects/FidelityFX-FSR) (MIT),
  [Qualcomm Snapdragon GSR](https://github.com/SnapdragonStudios/snapdragon-gsr) (BSD-3-Clause),
  [SMAA](https://github.com/iryoku/smaa) (MIT).
- Bibliotecas: [SDL](https://libsdl.org) (zlib), [Dear ImGui](https://github.com/ocornut/imgui) (MIT),
  [libadrenotools](https://github.com/bylaws/libadrenotools) (BSD-2-Clause), [volk](https://github.com/zeux/volk) (MIT),
  [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) (MIT),
  [shaderc](https://github.com/google/shaderc) (Apache 2.0), [FFmpeg](https://ffmpeg.org) (LGPL),
  [LZ4](https://github.com/lz4/lz4) (BSD-2-Clause), [Capstone](https://www.capstone-engine.org) (BSD-3-Clause),
  [Unicorn](https://www.unicorn-engine.org) (GPLv2, compilado do código-fonte por `android/deps/build_unicorn.sh`),
  [Python](https://www.python.org) (PSF) e [Zig](https://ziglang.org) (MIT) para a compilação no celular.
- O driver Turnip do [Mesa](https://www.mesa3d.org) (MIT), que os jogadores carregam por conta própria.
- Fontes: [Baloo 2](https://github.com/EkType/Baloo2) (OFL), DejaVu Sans (licença Bitstream Vera). O robô do Android
  é reproduzido ou modificado a partir de trabalho criado e compartilhado pelo Google (CC BY 3.0).
- Feito com o [Claude Code](https://claude.com/claude-code) (Anthropic).

*The Legend of Zelda* e *Breath of the Wild* são marcas da Nintendo. Este projeto não é afiliado nem endossado pela
Nintendo.
