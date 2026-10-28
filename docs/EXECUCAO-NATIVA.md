# Rodar o código do jogo direto na CPU (estudo, 2026-10-27)

Hoje o código ARM64 do BOTW é **recompilado**: `tools/gen.py` e `lifter.py` traduzem cada função para C, e o celular
compila esse C com o zig (`botwgame.so`, ~120 mil funções, 10-15 min, até 8 GB de memória). O Switch e o celular são
ARM64; o código original poderia rodar direto, como fazem o MacNativeGames e o totk-mobile na Apple. Este documento
diz o que isso mudaria aqui, o que se aproveita, os riscos e um protótipo com critério de parar.

## Por que olhar para isso agora

- **O limite dos 60 fps é a CPU do jogo** (docs/BOTW-PORT.md, "O que a thread que apresenta espera"): na 1.9.0 a
  thread de desenho do jogo espera ~16 ms por quadro pela thread principal, que roda ~12-15 ms de lógica do jogo
  numa thread só. A GPU (~14 ms) e a nossa thread de render (~12 ms) não são o limite: tirar 0,5 ms da GPU não mudou
  o período, nem as dicas ADPF, nem prender threads nos núcleos prime, nem tirar o revezamento do núcleo 0.
- Código recompilado para C e compilado a -O1 roda mais devagar que o código original (compilado pela Nintendo a -O2
  ou -O3): flags emuladas, registradores num struct, SIMD por helpers, cada desvio indireto por tabela. Quanto mais
  devagar não está medido; a ordem de grandeza esperada é 1,5-3x.
- A compilação no celular é a maior fonte de issues (#25, #47, #51, #52, #53: memória, ~80%, espaço). Rodando o código
  original, ela deixa de existir: a configuração só copia os arquivos.

## O que os outros fazem

| Projeto | Licença | Estado | Como roda o código |
| --- | --- | --- | --- |
| [MacNativeGames](https://github.com/ly05010419/MacNativeGames) (ly05010419) | MIT | TOTK e BOTW jogáveis no Mac; iPhone/iPad | Código ARM64 direto, módulo reescrito antes de carregar |
| [totk-mobile](https://github.com/OJZen/totk-mobile) (OJZen) | Apache-2.0; o núcleo, MIT | TOTK 1.4.3 jogável no Mac, iPad abaixo de 60 | Idem; o núcleo vem do [d2r-mobile](https://github.com/jhaizhou-ops/d2r-mobile) (`core/tools/patch_nso.py`) |
| [MacNativeGames-Android](https://github.com/deivid22srk/MacNativeGames-Android) (deivid22srk) | MIT | Andaime de um dia (2026-10-21): app, Vulkan, testes; **o jogo nunca rodou** | Não chegou ao reescritor |

O reescritor do totk-mobile (`patch_nso.py`) troca, no módulo, só o que não pode rodar fora do Switch:

- `x18` (registrador de plataforma) lido ou escrito → um slot por thread;
- `mrs/msr tpidr_el0` (o TLS do usuário) → um slot por thread;
- `mrs tpidrro_el0` (o ThreadLocalRegion do Horizon) → um slot por thread;
- `svc #n` e `mrs cntpct_el0` → uma "porta" para o host (um desvio para um stub que salva os registradores e chama C);
- remendos registrados por build (os bytes originais conferidos antes de trocar), como o nosso `tools/patches.py`.

O MacNativeGames-Android não tem código que sirva, mas a análise dele (`docs/android-port-analysis.md`) é uma boa
lista de verificação para o Android: memória executável anônima (`mmap` + `mprotect`, permitida ao `untrusted_app`,
como nos JITs), páginas de 16 KB, guest abaixo de 2^39, aliasing com `memfd`.

## O que aqui já serve como está

O runtime foi feito para o código recompilado, mas quase tudo que fica em volta dele é independente:

- **Memória:** o guest já mora nos endereços dele (janela 0x100000000-0x420000000, `main` em 0x100000000) e o
  código recompilado lê ponteiros direto (`LD64(a)` é `*(u64 *)a`). O código original enxergaria a mesma memória.
- **Kernel e serviços:** `kernel.c` (threads, sincronização, núcleo 0), os serviços IPC, o FS e romfs, áudio, HID,
  DLC e saves recebem `svc` e chamadas com um `Cpu *` - uma porta em assembly monta esse struct com os registradores
  e chama o mesmo C.
- **NVN, Vulkan, shaders, pacing, widescreen, upscalers, app Android, launcher:** nada muda; as entradas NVN que o
  jogo pega por `nvnBootstrapLoader` passariam a ser thunks em assembly para os mesmos `NVN(...)`.

## O que muda

| Parte | Hoje (recompilado) | Rodando direto | Esforço |
| --- | --- | --- | --- |
| Código do jogo | `botwgame.so` compilado no celular | O `main`/`sdk`/`subsdk` originais, reescritos no carregamento e mapeados executáveis no endereço deles | Grande: o reescritor (Python no PC hoje; no celular, C ou o Python que o app já leva) |
| `svc`, `x18`, TLS, contador | Viram C no lifter | Reescritos para portas e slots, como o d2r-mobile | Médio |
| Chamadas host → guest | `guest_call` pela tabela de funções | Um stub que carrega os registradores do `Cpu` e salta para o endereço | Pequeno |
| `tools/patches.py` (4-6 por versão) | C no lugar da instrução | Trampolins no binário (desvio para um stub que faz o remendo e volta) | Médio |
| `setjmp`/`longjmp` (`hle/setjmp.c`) | HLE, porque C não volta no meio de uma função | Desnecessário: o original funciona | Some |
| SIMD, flags, monitores exclusivos, FPCR | Emulados | Exatos, da própria CPU | Some (e corrige bugs que só a emulação tem) |
| Reinício para a tela inicial | `rt_unwind_point` na entrada de cada função | Não há pontos no código: parar as threads nos `svc` e por sinal | Médio, com risco |
| Revezamento do núcleo 0 | Nos `svc` e no `FN_ENTER` | Só nos `svc` (é onde o Horizon troca de thread também) | Pequeno |
| `fnprof`, `[slow]`, relatório de crash | Nomes `f_<endereço>` | Endereços do guest direto; perfis por amostragem (`simpleperf` no próprio PC do guest) ficam mais simples | Pequeno |

## Riscos

1. **`x18` e o ShadowCallStack.** No Android o `x18` é reservado; bibliotecas com ShadowCallStack o usam como
   ponteiro. Qualquer `x18` do jogo precisa ser reescrito, e um sinal (o rastreio de escrita de páginas usa SIGSEGV)
   que chegue com o jogo rodando não pode depender dele. A reescrita do d2r-mobile cobre isso.
2. **`tpidr_el0`.** A bionic guarda o TLS dele aí; o jogo não pode escrever nele. Reescrito para slot; os stubs
   restauram o do host ao entrar em C.
3. **`cntpct_el0`.** O kernel pode não deixar o espaço de usuário ler o contador físico; o virtual (`cntvct_el0`) sim, e
   o do Snapdragon também é de 19,2 MHz, a frequência do Switch. Reescrito como no d2r-mobile, ou para `cntvct_el0`.
4. **Memória executável.** Mapeamento anônimo com `PROT_EXEC` depois de escrito: permitido para apps (é o caminho dos
   JITs), mas cada fabricante pode ter política própria. É a primeira coisa a provar no POCO e num Mali.
5. **Páginas de 16 KB** (alguns Android 15+): os segmentos do NSO são alinhados a 4 KB; o carregador precisa
   tolerar isso (mapear a região inteira e copiar).
6. **O que o rastreio de escrita de páginas (`mprotect` + SIGSEGV) faz com código nativo:** hoje o handler sabe que a
   escrita veio do código recompilado; com o original, o PC da falta está no código do jogo - o handler precisa
   continuar funcionando com `x18`/TLS do guest ativos.
7. **Tamanho do trabalho.** É trocar o coração do runtime. O caminho recompilado tem que continuar existindo (outros
   aparelhos, depuração, a versão que já funciona) atrás de uma chave, pelo menos até o nativo se provar.

## Protótipo proposto (com critério de parar)

Num branch, atrás de `LARECOMP_NATIVE=1`, na 1.0.0 (a versão mais simples) e depois na 1.9.0:

1. **P0 - executar.** O app mapeia o `sdk` original reescrito (só `svc`, `x18`, TLS, contador) num endereço fixo, com
   `PROT_EXEC`, e chama `nn::init::Start` de uma thread com os slots preparados. Sucesso: o primeiro `svc` chega ao
   `kernel.c`. Prova as perguntas 1-5 dos riscos no POCO (Adreno) e, se possível, num Mali.
2. **P1 - até a tela de título.** `main` e `subsdk` também; as entradas NVN como thunks; `patches.py` da 1.0.0 como
   trampolins. Sucesso: a tela de título com som e o menu.
3. **P2 - medir.** O mesmo save da 1.9.0 no Grande Platô, `LARECOMP_PROF_SVC_TID` e `[pace]`: o tempo da thread
   principal por quadro e os fps contra o recompilado, no mesmo calor. **Parar se o ganho for pequeno** (a thread
   principal menos de ~25% mais rápida): o custo de manter dois caminhos não se paga.
4. **P3 - completar.** Reinício para a tela inicial, relatório de crash, as outras versões, DLC, e só então trocar o
   padrão (a compilação no celular vira o caminho de reserva).

## Resultado do P0 (branch `native-p0`, 2026-10-27, POCO F7 Ultra, 1.9.0)

`LARECOMP_NATIVE=1` (`runtime/native.c`, `runtime/native_gate.S`): o `main` e o `sdk` originais, reescritos no
carregamento, rodam direto na CPU. **O jogo chegou à tela de título e ao mundo** (o save do Grande Platô, a imagem
certa, sem crash), sem usar o `botwgame.so`.

- Reescrito: 152 `svc` (stubs que entram em `native_svc_gate`), 129 leituras/escritas de `tpidrro_el0`/`tpidr_el0`
  (stubs que acham o `Cpu` da thread por `TPIDR_EL0` numa tabela com sondagem), 43 `mrs cntpct_el0` (viram
  `cntvct_el0`: o contador do Snapdragon é de 19,2 MHz, o mesmo do Switch). O `x18` ficou com o jogo (~38.000 usos):
  o Linux o preserva, e as portas devolvem o do host enquanto o C roda.
- As funções do runtime (HLE, NVN) viraram código nos endereços falsos de `HLE_BASE`; `guest_call` entra pelo
  `native_enter`. `kernel.c`, serviços, NVN e Vulkan não mudaram.
- Bugs achados: (1) chamadas aninhadas (o runtime chamando o jogo de dentro de uma porta) perdiam os registradores
  preservados - o `native_exit` agora os devolve ao `Cpu`; (2) duas threads no mesmo slot da tabela - agora com
  sondagem.
- **Desempenho: sem ganho.** Rodadas alternadas, mesmo save, mesmo calor (~62-66 °C): nativo 51,5 e 52,2 fps,
  recompilado 55,1 e 55,4 fps (render ~11,8 e GPU ~14,3 ms nos dois). A thread principal continua ~70% ocupada
  (~13 ms por quadro) fora das esperas de svc, como no recompilado: o tempo dela não é o código do jogo em si. O
  mais provável é a espera ativa pelos workers de job (a `t80` gira em `SleepThread(-1)` e `WaitForAddress` esperando
  os jobs, que também já rodavam bem recompilados) e o custo das portas (cada `svc` salva ~800 bytes e passa pelo
  revezamento do núcleo 0).
- Pelo critério do protótipo (a thread principal 25% mais rápida), **o caminho nativo não se paga como ganho de fps**.
  O que ele daria é outra coisa: nenhuma compilação no celular (sem os 10-15 min, sem o fim por falta de memória das
  issues #25/#47/#51/#53), menos memória e menos espaço. Isso fica para decidir; faltariam os `patches.py` como
  trampolins (60 fps, widescreen, as correções de cada versão), o reinício para a tela inicial e o relatório de crash.

## Depois do P0: sem compilação no celular (2026-10-27)

- **Configuração** "Native game code (experimental)" (Vídeo, Android; `native_code` no ini, desligada por padrão;
  `LARECOMP_NATIVE=0/1` passa por cima): com ela o boot não carrega o `botwgame.so` e a configuração dos arquivos do
  jogo pula a compilação (o launcher não pede mais "The game code needs building").
- **As correções do `tools/patches.py`** viraram stubs (`stub_fix`, `native_patch_gate`): o handler em C trabalha no
  `Cpu` e escolhe como seguir - a instrução original (copiada no stub), pular ela, o outro endereço da correção, ou
  `ret`. As 12 (1.0.0: 2, 1.6.0: 2, 1.8.2: 4, 1.9.0: 4) conferem a palavra original antes de trocar.
- **Reinício** (`LARECOMP_REBOOT_AT=3600:game`): 45 threads paradas em 20 ms, o jogo desmontado e montado de novo,
  nativo outra vez até a tela de título. As threads param nos `svc` e nas chamadas ao runtime, como antes.
- **Relatório de crash:** uma falha dentro do código do jogo copia os registradores do sinal para o `Cpu` (o
  relatório mostra os do jogo e a cadeia de chamadores) e nomeia o pc pelos símbolos dos módulos (`main+offset`).
- **Testado:** 1.9.0 (título, mundo, reinício) e 1.0.0 (título, mundo). Uma rodada da 1.0.0 caiu depois de ~25 s no
  mundo (`nvnBufferGetAddress` recebeu do jogo um objeto com 0x600000001 no lugar do ponteiro); a mesma rodada de novo
  passou de 2 min sem cair: uma corrida que o tempo do nativo expõe. A 1.6.0 e a 1.8.2 não foram testadas.

## Em jogo pelo usuário (2026-10-27, 1.9.0, Sync lag desligado)

- 58-60 fps estáveis em boa parte, com a CPU a ~1 GHz e 35% de uso; mais quedas que o recompilado.
- **Correção, mesmo dia:** as pontas na nuvem e na névoa num golpe **não são do nativo**. O usuário as viu no
  recompilado também, com e sem Sync lag, a 30 e a 60 fps, sem o arredondamento das coordenadas de textura
  (`LARECOMP_NO_VS_SNAP=1`) e nas versões publicadas 0.2.1 e 0.2.0 (recompiladas com versionCode 19 e 20 para o
  teste): um defeito antigo que só não tinha sido notado. As cortinas também são o bug antigo. O que segue é o relato
  como foi feito:
- **Defeitos só do nativo** (no recompilado, na mesma sessão, nada disso):
  - Num golpe, a nuvem da Montanha da Morte (e a névoa do horizonte) se estica de lado em pontas por **um quadro**
    (gravação 19:30, ~1:11, junto da faísca do acerto). O Link pisca às vezes ao levar ou dar o primeiro golpe.
  - As **cortinas** do terreno ([[botw-stretched-vertices]]) aparecem com frequência: na conversa com o velho logo
    depois da torre e ao longe no Grande Platô. Para esse bug antigo o modo nativo é quase uma reprodução garantida.
- Descartados: o Sync lag (desligado, continua), o caminho dos 64 KB (desligado), o FPCR (o jogo liga o FZ, bit 24,
  nas suas threads - o recompilado também respeita), uma espera por cerca ainda não enviada (nunca aconteceu).
- Próximo passo, se for retomado: um quadro de depuração (F9) no instante do golpe com e sem o nativo, comparando os
  constant buffers da nuvem; para as cortinas, a mesma captura na conversa depois da torre.

Créditos, se algo for aproveitado: o reescritor e os stubs do d2r-mobile (MIT, com o aviso dele), as ideias do
totk-mobile (Apache-2.0: o aviso de licença e um NOTICE se houver código) e do MacNativeGames (MIT), no README.
