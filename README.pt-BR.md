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
>   junto dos menus existentes em inglês e português. Os menus seguem a opção Idioma do jogo;
>   Auto usa o idioma do aplicativo no Android.
> - **Suporte a futuras traduções:** catálogos separados, recursos de texto Android e verificações na CI facilitam
>   a inclusão de outros idiomas. Veja o [guia de tradução](docs/TRANSLATIONS.md).
> - **Atualização BOTW 1.9.0 (experimental):** tela de título, carregamento de saves e mundo aberto testados no
>   Odin 2 Portal. A instalação em `v190` tem seus próprios saves e configurações. Os gráficos e o desempenho ainda
>   precisam de melhorias; selecionar 60 fps não garante 60 fps constantes.
> - **Controles e inicialização:** o remapeamento vale para o jogo e os menus do app; uma tela de carregamento
>   traduzida acompanha a inicialização. O suporte widescreen do projeto original está integrado para 1.8.2 e 1.9.0 experimental.
>
> O motor de recompilação e o suporte existente ao jogo vêm dos projetos de origem creditados abaixo. As traduções
> dos menus descritas aqui são da interface do aplicativo; o idioma dos textos do jogo tem sua própria configuração.

Um porte de *The Legend of Zelda: Breath of the Wild* (Nintendo Switch, versões **1.0.0, 1.6.0, 1.8.2 e 1.9.0**) para
Android. **Não é emulação do Switch**: o código do jogo pode ser recompilado em C para o celular ou, no modo nativo
experimental, executado diretamente na CPU ARM64. A API gráfica do jogo (NVN) é traduzida para Vulkan.

O projeto parte do motor do **[LAS Recomp](https://github.com/peiikai/LASRecomp)**, de peiikai (uma recompilação de
*Link's Awakening*), e do seu porte para Android, o **[LASRecompAndroid](https://github.com/mamcjr/LASRecompAndroid)**.

> [!WARNING]
> **Beta.** Testado principalmente em um celular (POCO F7 Ultra, Adreno 830, driver Mesa Turnip). O desempenho e a
> compatibilidade variam entre aparelhos; espere bugs, sobretudo em GPUs ainda não testadas fisicamente. Veja as
> [notas da 0.2.3-beta](https://github.com/mamcjr/BOTWRecompAndroid/releases/tag/v0.2.3-beta) e as
> [notas de desenvolvimento](docs/BOTW-PORT.md).

## Plataforma suportada

Android é o único alvo do aplicativo. Os ports e pacotes desktop foram descontinuados em 2026-10-22.
Windows, Linux e macOS podem continuar sendo usados como ambientes de desenvolvimento com o Android SDK/NDK.

## Estado

- **Jogável** da tela de título até o mundo, com saves, no POCO F7 Ultra testado. As quatro versões listadas são
  suportadas no modo recompilado. O modo nativo foi testado com 1.0.0 e 1.9.0; 1.6.0 e 1.8.2 ainda precisam de testes
  nesse modo.
- **Código do jogo:** escolha o modo Recompilado (compila o código uma vez no celular) ou Nativo experimental (dispensa
  essa compilação). Quem configurou o modo Recompilado pode alternar para o Nativo depois.
- **Arquivos do jogo:** copie os dados extraídos para a pasta do app ou, desde a **0.2.3-beta**, use-os no lugar, sem
  uma cópia extra de cerca de 16 GB. Usar no lugar exige acesso a todos os arquivos do Android, e a pasta original
  precisa continuar disponível.
- **DLC, saves e mods:** instale os dados extraídos do seu Passe de Expansão; importe ou exporte saves em ZIP ou importe
  uma pasta de saves de emulador; use mods, inclusive os que adicionam arquivos e pastas de mod do DLC. O uso de mods
  no lugar ainda precisa de mais testes.
- **Vídeo e controles:** opção de 60 fps nas versões do jogo compatíveis e cutscenes a 60 fps experimentais,
  proporção livre da tela, resoluções interna e de saída, upscaling FSR 1 ou Snapdragon GSR 1, FXAA/SMAA, controles de
  toque e físicos e giroscópio. O app oferece escolha de driver Vulkan e opções de compatibilidade.
- **Correções da 0.2.3:** pontos e linhas escuras na grama, pontas na fumaça após golpes, texto incompleto de GAME
  OVER, água branca com Sync lag ligado antes do carregamento, problemas de interface na proporção livre e alguns
  crashes. A correção de memória compartilhada nas GPUs Mali ainda não foi confirmada em um aparelho físico.
- **Problemas conhecidos:** a grama baixa só aparece de perto; reflexos da água podem acompanhar a câmera; o fundo
  do menu de pausa pode ficar preto no driver padrão da Qualcomm; Link pode piscar no primeiro golpe no modo nativo.
  Depois da ativação de uma Torre Sheikah, o terreno pode esticar em "cortinas"; recarregue um save feito após a
  ativação.

> [!IMPORTANT]
> Como o LAS Recomp, este projeto é feito quase todo com IA (Claude, da Anthropic, pelo Claude Code), dirigido,
> testado e revisado por um humano.

> [!NOTE]
> **Nenhum arquivo do jogo ou DLC vem incluído.** Você precisa dos seus próprios arquivos do jogo, obtidos legalmente
> e extraídos (`exefs` e `romfs`). O app não abre NSP/XCI nem usa `prod.keys`. Não serão dadas instruções de como
> obter os arquivos.

### Usando a atualização 1.9.0

Importe as pastas `exefs` e `romfs` correspondentes do seu jogo com a atualização **1.9.0 já aplicada**.
O APK importa arquivos extraídos; não aplica uma atualização diretamente a um XCI ou NSP.
A versão é detectada automaticamente e instalada em `v190` no armazenamento escolhido, inclusive no cartão SD.
Selecione as versões instaladas em **Configurações → Arquivos**. Cada versão tem saves e configurações
separados. Ao criar uma pasta de versão, o app pode copiar os saves e configurações da 1.0.0 sem substituir os existentes.

## Compatibilidade dos controles

**Controles integrados — AYN Thor, Odin 2 e Retroid:** os botões padrão, os dois analógicos e os gatilhos devem
funcionar pelo suporte a gamepads Android do SDL3, desde que o aparelho seja compatível com o APK. O **Odin 2 Portal
foi testado**, incluindo confirmar/voltar remapeados nos menus do app; AYN Thor e Retroid ainda não foram testados.
Abra **Configurações → Controles** para verificar a detecção e remapear os botões do jogo e dos menus do app.
A disposição padrão é a da Nintendo: **A à direita, B embaixo**.

## Instalando (beta)

1. Baixe o APK em [Releases](https://github.com/tib-kpl/BOTWRecompAndroid/releases) e instale (Android 11+, ARM64 e
   Vulkan). Tenha a sua própria pasta extraída com `exefs` e `romfs`.
2. Em **Arquivos do jogo**, escolha **Código do jogo**: **Nativo (experimental)** dispensa a compilação no celular;
   **Recompilado** compila uma vez. Em **Arquivos do jogo**, escolha **Copiar** ou **Usar no lugar** e selecione a pasta
   extraída. Usar no lugar pede acesso a todos os arquivos do Android; mantenha a pasta no mesmo local. Copiar exige
   cerca de 16 GB adicionais para os dados do jogo, e o modo Recompilado também precisa de espaço para o código gerado.
3. Se você possui o Passe de Expansão, instale o `romfs` extraído do DLC em **Configurações > DLC > Instalar DLC**.
   Para trazer saves, use **Configurações > Saves** para importar um ZIP ou uma pasta de saves de emulador.
4. Em celulares com Adreno, um driver Mesa Turnip pode ter desempenho melhor que o driver do sistema. Selecione-o em
   **Configurações > Arquivos > Driver Vulkan**. Se o app informar que o driver Vulkan é antigo ao abrir, toque e
   segure o ícone do app e escolha **Escolher driver** antes de iniciar o jogo.

Atualizar da 0.2.x para a 0.2.3-beta não exige recompilar o código do jogo. Os shaders são compilados novamente uma
vez, então os primeiros minutos podem ter travadinhas. Para relatar um problema, abra uma
[issue](https://github.com/mamcjr/BOTWRecompAndroid/issues) com celular, GPU, driver, versão do jogo e um log de
**Configurações > Arquivos > Salvar um arquivo de log**.

## Testando no Android (desenvolvedores)

Com o Android SDK / NDK, um celular no adb e o seu dump em `Dump/`:

```bash
cd android/botwrecompandroid && ./gradlew assembleDebug    # o app
android/build_gamelib.sh Dump/01007EF00011E000/exefs  # o código do jogo (cerca de uma hora)
android/push_game.sh Dump/01007EF00011E000           # arquivos e código do jogo para o celular
android/run_boot.sh 120                               # abre o jogo, log em android/out/boot.log
```

## Apoie o projeto

O BOTWRecompAndroid é um projeto gratuito e independente. Se você gosta dele e quer apoiar seu desenvolvimento, pode
me patrocinar pelo GitHub. Esse apoio me ajuda a dedicar tempo à correção de bugs, à melhoria do desempenho e da
compatibilidade e às próximas versões. O patrocínio é totalmente opcional: não libera recursos exclusivos nem garante
correções específicas. Relatos de bugs, testes em outros aparelhos e contribuições também ajudam muito.

[![Patrocine no GitHub](https://img.shields.io/badge/Patrocine-no%20GitHub-EA4AAA?logo=githubsponsors&logoColor=white)](https://github.com/sponsors/mamcjr)

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
