# Rumo aos 60 fps no POCO F7 Ultra (Turnip): medições de 23/10/2026

Cena: 1.8.2, Grande Platô perto do Templo do Tempo (save de teste), Video > Frame rate **60**, resolução interna 1x.
Aparelho: Adreno 830, Mesa Turnip 26.3-devel.

## Como medir sem se enganar

- **Use `frame_rate = 60` no arquivo de teste.**
  - Com `frame_rate = 30` (o padrão do app), o pacer finge contadores de GPU lentos.
  - O jogo então fica na resolução dinâmica mínima, mesmo com `LARECOMP_FPS=60`.
  - Os testes de antes desta noite estavam nesse modo misto.
- **O celular esquenta (60–90 °C) em poucos minutos e, carregando, não esfria abaixo de ~50 °C.**
  - Duas rodadas seguidas diferem mais do que a maioria das mudanças.
  - Use **`LARECOMP_AB=<quadros>:<chave>`**: alterna uma chave a cada N quadros dentro da mesma rodada, e o log
    `[pace]` marca `ab 0/1`. Os dois lados ficam no mesmo calor.
  - Chaves: `split`, `ubo`, `sgsr`, `nobarrier`, `copy`, `chunk`.
- **`LARECOMP_GPUPROF_AT=<quadro>`:** tempo de GPU por passe. Os valores absolutos ficam inflados pelos timestamps,
  mas as proporções valem.
- **`simpleperf`** funciona no app (debuggable). Símbolos: a `libmain.so` de `merged_native_libs` (-O2 -g).

## O que mudou e quanto rendeu

Medições na mesma rodada (A/B):

| Mudança | Antes | Depois |
| --- | --- | --- |
| Leitura no lugar em vez de cópia de feedback (9d0ea24, 1725400) | 31,2 fps, GPU 22,4 ms | **40,5 fps, GPU 16,6 ms** |
| SGSR 1 em vez de FSR 1 (padrão novo no Android, 6a67bd8) | 35,9 fps, GPU 19,7 ms | **39,0 fps, GPU 18,3 ms** |
| Constant buffers lidos no lugar (`LARECOMP_UBO_DIRECT=1`) | render 13,0 ms | render 12,4 ms (GPU igual) |

- **Cópias de feedback:** os decalques do terreno (fs `f984e489`) leem e escrevem dois alvos do G-buffer, com uma barreira
  a cada um ou dois draws. Eram 32 pares de cópias de tela cheia por quadro, ~14 ms de GPU. Agora o passe só termina onde
  havia cópia, e o draw lê o próprio alvo. É um feedback loop que o Vulkan deixa indefinido sem
  `VK_EXT_attachment_feedback_loop_layout`, então fica **só no Turnip**, onde foi conferido quadro a quadro. Os outros
  drivers mantêm as cópias, ou usam `LARECOMP_FEEDBACK_INPLACE=1`.
- **Traços pretos na grama (b9321e7):** eram do FSR 1. A saída intermediária A2B10G10R10 comprimida (UBWC) devolvia
  blocos 16x4 de zeros ao RCAS no Turnip. Agora essa imagem é criada sem compressão. Antes, eram ~1 por quadro.

## Testes sem efeito (os padrões ficam)

- **Resolução 50%:** GPU igual (~18 ms). O custo não é por pixel.
- **Saída final em 1080p em vez de 1440p** (`LARECOMP_OUT_LINES=1080`): ≤ 1 ms.
- **Bilinear em vez de SGSR:** dentro do ruído.
- **`TU_DEBUG=sysmem`:** igual ao autotune. **`TU_DEBUG=gmem`:** muito pior (GPU ~30 ms).
- **Matemática de shader em 32 bits** (`LARECOMP_VK_INT32=1`): igual ao int64 emulado.
- **`IR3_SHADER_DEBUG=nouboopt`:** igual.
- **Sem a barreira no início de cada passe** (`nobarrier`): igual.
- **Sem dividir o passe longo** (`split`): ~0,5 ms a menos de GPU, mas o quadro fica mais longo, porque a GPU começa
  depois.
- **Submissões a cada 0,5 ms** (`chunk`): igual.
- **Janela compartilhada para os constant buffers de 64 KB:** pior. São sempre o mesmo endereço, com 16 bytes
  reescritos antes de cada draw (+0x70). Cada bind precisa da própria cópia, ~14 MB por quadro de memcpy.

## Onde está o limite agora

- **O jogo espera o fence do quadro anterior a cada quadro** (`LARECOMP_LOG_SYNC`, lr `100d81418`).
  - São 8,5–9 ms esperando a GPU; a tradução já passou desse ponto.
  - A GPU (~17 ms de trabalho, mais ~3 ms parada no início de cada quadro) é o que segura o jogo em ~40 fps.
  - A tela é de 120 Hz: entre 16,7 e 25 ms, o quadro cai em 25 ms.
- **Para 60 estáveis aqui, o trabalho de GPU precisa ficar em ~13–14 ms**, ~25% a menos. O custo é por draw (~2 µs)
  e por passe (~20–40 µs; são ~190 passes por quadro), não por pixel.
- **Thread de render:** ~12–13 ms por quadro. Não é o limite hoje, mas está perto. Maiores itens no `simpleperf`:
  - `draw_` 13%;
  - descriptor sets 12%;
  - memcpy dos constant buffers 12%;
  - `begin_pass` 9%.

## Perfil por passe com widescreen (tarde de 23/10)

Grande Platô, modo 60, widescreen 3200x1440, cena 1x (1600x720), `LARECOMP_GPUPROF_AT=1700`. Com as marcas de
tempo o quadro mede ~26 ms de passes (mais que os ~17 ms sem elas):

| Item | Tempo | Observação |
| --- | --- | --- |
| G-buffer (6 cores + profundidade) | 10,4 ms | 55 passes, ~2.460 draws; o grosso do quadro |
| Passes de 1 cor em 1920x1080 (vão a 3200x1440) | ~3,4 ms | composição final; o último inclui a GPU parada até a próxima submissão |
| Upscale da cena (SGSR 1, 1600x720 → 3200x1440) | 2,0 ms | |
| Sombras (só profundidade) | 1,7 ms | 11 passes, 526 draws |
| Decalques lidos no lugar (`f984e489`, `69c0a04e`) | ~0,9 ms | ~40 passes de 1 draw, ~17 µs cada (sysmem) |

- O contador novo no log de fps (`fbcopy ... breaks N`) mostra ~80 quebras de passe por quadro para leituras no lugar.
- **Conclusão:** tirar essas quebras ganharia menos de 1 ms (~3%). Não compensa o risco de um feedback loop dentro do
  passe. O tempo está na geometria do G-buffer, no upscale e na composição na resolução de saída.

## Compatibilidade com outras GPUs (23/10)

Caminhos seguros (memória do jogo por cópias da CPU, cópias em vez de leitura no lugar, LRZ desligado), simulando
uma GPU que não é Adreno 8xx: o mundo desenha certo, mas a **15 fps** contra 35 (5,5 GB/s de cópias). Por isso o
Automático da "Compatibilidade do driver" só usa os caminhos seguros fora das Adreno 8xx.

## Próximos passos possíveis

1. **Resolução de saída menor no widescreen:** a composição final e o upscale rodam em 3200x1440; uma opção de saída
   menor (com o HUD menos nítido) cortaria parte dos ~5 ms. Os passes de decalque (acima) não valem a pena.
2. **Medir o clock da GPU** (contadores do `VK_KHR_performance_query` do Turnip), para separar trabalho de
   governador de frequência.
3. **Geração de quadros (30 → 60)** para GPUs que não chegam a 60: ver `FRAMEGEN-AVALIACAO.md`.

## 26/10: saída Automática, GPU parada e a thread de render

Mesmo celular, 1.8.2 no Grande Platô, as configurações de um jogador (50%, FSR 1, Saída: Tela, 3200x1440).

- **Saída Automática (60ac287):** a imagem final em 1080 linhas nas telas maiores; a cena mantém o seu tamanho.
  GPU 22,8 → ~17,6 ms por quadro. O passo de apresentar passou a ser bilinear (0,13 ms; o SGSR 1 ali custava 1,9 ms
  sem diferença visível em 1,33x).
- **Linha do tempo de um quadro (`LARECOMP_FRAME_TIMELINE`, 6a59793):**
  - o jogo simula ~16 ms;
  - espera ~4–6 ms o fence do quadro que **acabou** de enviar;
  - grava e envia a lista do quadro em ~2,6 ms, com a GPU parada;
  - a thread de render traduz em ~11–13 ms, submetendo a cada 2–3 ms;
  - a GPU termina ~9 ms depois da última submissão.
- **GPU parada (`LARECOMP_LOG_GPU_IDLE`):** ~4 ms por quadro dentro do quadro, esperando a tradução, e ~2,7 ms entre
  quadros, esperando o jogo gravar.
- **A thread de render está no caminho crítico, mas o perfil é plano:**
  - ~2.400 draws por quadro, ~6 µs cada;
  - o maior item é o memcpy dos constant buffers dinâmicos (~10%): ~16 MB por quadro, ~22 KB cada, porque o jogo
    reescreve 16 bytes antes de cada draw.

### Testes sem efeito (26/10)

- **Micro-otimizações da tradução (be08e10):** `draw_writes` uma vez por draw, hash da chave em palavras, `AB()` sem
  chamada. Render 14,85 → 14,62 ms, dentro do ruído. O "laço quente" de `draw_writes` no simpleperf era deslize de
  amostragem.
- **Thread de cópias** (as cópias ≥ 2 KB dos constant buffers em outro núcleo, em ordem):
  - cópias pendentes recebem um patch com os bytes antigos quando o jogo escreve por cima, e a fila esvazia antes de
    cada submissão;
  - a imagem fica certa, e ~12 MB por quadro passam pela thread;
  - a thread de render espera ~0,3–1 ms por quadro na barreira;
  - A/B na mesma rodada: render 13,52 ms sem, 13,82 com. Revertido.
- **Fixar a thread principal do jogo num núcleo prime:** já testado em 16/10 (`LARECOMP_PIN`). Derruba o jogo, por uma
  corrida no sistema de jobs.

### Onde ainda há ganho

1. **O jogo uma frame adiante** (liberar o fence antes de a GPU terminar). Elimina os ~2,7 ms entre quadros e
   sobrepõe a tradução à GPU do quadro anterior. O risco é o jogo reescrever dados de vértice lidos no lugar enquanto
   a GPU ainda os usa.
2. **Os 16 bytes por draw como push constants:** o tradutor de shaders leria esse trecho do constant buffer de outro
   lugar, e as cópias de 64 KB por draw acabariam.
3. **Os passes da cena na GPU** (o G-buffer, ~10 ms).
4. **A simulação do jogo (~16 ms por quadro):** sozinha, já limita a ~53 fps nesta cena.

### Mais testes de 26/10 (lista de otimizações e o SavageWindRecomp)

- **`-O3` + ThinLTO na libmain:** pior. 31,8 e 30,5 fps contra 34,6 e 35,5 com `-O2`, em duas rodadas de cada.
  Provavelmente pelo código maior no cache de instruções. Fica `-O2`.
- **`SleepThread(0)` dormindo 50 µs depois de 32 yields seguidos:**
  - +1% de fps, dentro do ruído;
  - CPU do processo 2,51 → 2,47 núcleos, sem diferença de temperatura;
  - o giro com `sched_yield` custa menos do que os 3,3% do perfil sugeriam. Não entrou.
- **Textura consultada uma vez por draw:** a parte duplicada é ~0,5% da thread de render, abaixo do que uma medição
  separa. Não entrou.
- **GLSL mais barato do SavageWindRecomp (9f94aba):** built-ins nas conversões, só as funções do prelúdio que o shader
  usa e SPIR-V sem nomes. Com os caches frios:
  - espera por shaders 17,3 → 11,2 s;
  - pior quadro 6,2 → 3,8 s;
  - a mesma imagem.
- **Memória de comandos (#74 do SavageWindRecomp):** não se aplica aqui.
  - Depois de 2 min no mundo, 43 páginas de sombra (21 MB de endereço, menos em RSS), num processo de 2,2 GB.
  - Os pools do BOTW têm 16 KB, menos que uma página de sombra de 64 KB, então liberar páginas inteiras quando um pool
    fecha não liberou nenhuma em ~300 finalizações.
  - A sombra aqui é por página de memória do jogo e se reaproveita quando o jogo reaproveita a memória. Revertido.
