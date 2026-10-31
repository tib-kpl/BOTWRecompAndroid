**Beta 17.** O desempenho da Mali de volta, sem perder as correções da 0.2.3, e a compilação do código do jogo que falhava sempre em alguns celulares.

## Novidade

- **Um romfs.bin no lugar de ~41 mil arquivos ([#87](https://github.com/mamcjr/BOTWRecompAndroid/issues/87)):** copiar a pasta romfs/ inteira para o celular é lento e falha em muitos aparelhos. Agora a pasta escolhida pode ter `exefs/` e um **`romfs.bin`** já pronto ao lado (feito no PC, por exemplo com `hactool --romfs=romfs.bin`, com a atualização aplicada por `--basenca`). O app confere a versão e se o arquivo está inteiro, e funciona tanto em **Copiar** quanto em **Usar no lugar**. A pasta romfs/ continua funcionando como antes.

## Correções

- **Mali lenta na 0.2.3 ([#88](https://github.com/mamcjr/BOTWRecompAndroid/issues/88), [#89](https://github.com/mamcjr/BOTWRecompAndroid/issues/89), [#93](https://github.com/mamcjr/BOTWRecompAndroid/issues/93), [#54](https://github.com/mamcjr/BOTWRecompAndroid/issues/54), [#61](https://github.com/mamcjr/BOTWRecompAndroid/issues/61), [#94](https://github.com/mamcjr/BOTWRecompAndroid/issues/94)):** para acabar com os crashes e o texto embaralhado, a 0.2.3 pôs a memória que a CPU e a GPU dividem no tipo **sem cache** da Mali. Ali cada leitura e escrita da CPU fica muito lenta, e o mundo caiu para 2-9 fps. Agora essa memória volta para o tipo com cache, e o app sincroniza os caches à mão. As correções da 0.2.3 continuam valendo. Testado num Adreno configurado como a memória da Mali; sem um celular Mali para testar: contem como ficou.
- **Compilação falhando em um segundo ([#52](https://github.com/mamcjr/BOTWRecompAndroid/issues/52), [#92](https://github.com/mamcjr/BOTWRecompAndroid/issues/92)):** quando a compilação retomava de onde tinha parado, ela lia o log da execução anterior e pedia ao compilador um arquivo que ainda não existia (`file_open FileNotFound`). Agora cada execução começa com um log novo e espera cada arquivo ficar pronto.
- **Giroscópio com controle USB-C ([#78](https://github.com/mamcjr/BOTWRecompAndroid/issues/78)):** com o GameSir X5 Lite, o giroscópio funcionava na primeira mira e depois parava. Uma leitura solta do controle fazia o app trocar para o giroscópio dele, que só manda zeros. Agora o do controle só assume depois de se mexer por um quarto de segundo, e o do celular volta se o do controle ficar 2 s em zero. Sem esse controle para testar: contem se resolveu.

> **Não precisa compilar o código do jogo de novo:** é só instalar por cima da 0.2.x.

## Problemas conhecidos

- Aspecto livre: na troca de páginas do menu, a página aparece cortada na borda da parte 16:9 enquanto desliza.
- A grama baixa ao longe só aparece de perto.
- Às vezes, logo depois da cena de ativação de uma torre, o terreno aparece esticado em "cortinas". Para resolver, carregue o save depois de a torre estar ativada.
- O reflexo na água acompanha a câmera quando o Link anda para os lados.
- No driver de fábrica da Qualcomm, o fundo do menu de pausa fica preto.
- Modo nativo: o Link às vezes pisca ao levar o primeiro golpe.

Nenhum arquivo do jogo vem incluído. Testada num celular só (POCO F7 Ultra, Adreno 830, Turnip). Para instalar e configurar: [0.1.0-beta](https://github.com/mamcjr/BOTWRecompAndroid/releases/tag/v0.1.0-beta). Créditos no [README](https://github.com/mamcjr/BOTWRecompAndroid#créditos).

---

**Beta 17.** Mali performance back, keeping 0.2.3's fixes, and the game code build that always failed on some phones.

## New

- **One romfs.bin instead of ~41,000 files ([#87](https://github.com/mamcjr/BOTWRecompAndroid/issues/87)):** copying the whole romfs/ folder to the phone is slow and fails on many devices. The picked folder can now hold `exefs/` and a ready-made **`romfs.bin`** beside it (made on a PC, e.g. with `hactool --romfs=romfs.bin`, the update applied with `--basenca`). The app checks its version and that it's whole, and it works with both **Copy them** and **Use them in place**. A romfs/ folder still works as before.

## Fixes

- **Mali slow in 0.2.3 ([#88](https://github.com/mamcjr/BOTWRecompAndroid/issues/88), [#89](https://github.com/mamcjr/BOTWRecompAndroid/issues/89), [#93](https://github.com/mamcjr/BOTWRecompAndroid/issues/93), [#54](https://github.com/mamcjr/BOTWRecompAndroid/issues/54), [#61](https://github.com/mamcjr/BOTWRecompAndroid/issues/61), [#94](https://github.com/mamcjr/BOTWRecompAndroid/issues/94)):** to end the crashes and garbled text, 0.2.3 put the memory the CPU and GPU share in Mali's **uncached** memory type. Every CPU read and write there is very slow, and the world dropped to 2-9 fps. Now that memory goes back to the cached type, and the app syncs the caches by hand. 0.2.3's fixes still hold. Tested on an Adreno set up like Mali's memory; there's no Mali phone to test on: please tell me how it runs.
- **Build failing after a second ([#52](https://github.com/mamcjr/BOTWRecompAndroid/issues/52), [#92](https://github.com/mamcjr/BOTWRecompAndroid/issues/92)):** when the build resumed where it had stopped, it read the previous run's log and asked the compiler for a file that didn't exist yet (`file_open FileNotFound`). Now each run starts with a fresh log and waits for each file to be ready.
- **Gyroscope with a USB-C controller ([#78](https://github.com/mamcjr/BOTWRecompAndroid/issues/78)):** with the GameSir X5 Lite, the gyroscope worked for the first aim and then stopped. One stray reading made the app switch to the controller's gyroscope, which only sends zeros. Now the controller's takes over only after moving for a quarter of a second, and the phone's comes back once the controller's has read zero for 2 s. There's no such controller here to test with: please tell me if it's fixed.

> **No need to build the game code again:** just install over 0.2.x.

## Known issues

- Free aspect: when the menu turns its page, the page shows cut at the 16:9 part's edge while it slides.
- Short grass in the distance only appears up close.
- Sometimes, right after a tower's activation scene, the terrain shows stretched into "curtains". To fix it, load the save once the tower is active.
- The water's reflection follows the camera when Link walks sideways.
- On Qualcomm's stock driver, the pause menu's background is black.
- Native mode: Link sometimes flickers on the first hit he takes.

No game files are included. Tested on one phone only (POCO F7 Ultra, Adreno 830, Turnip). To install and set up: [0.1.0-beta](https://github.com/mamcjr/BOTWRecompAndroid/releases/tag/v0.1.0-beta). Credits in the [README](https://github.com/mamcjr/BOTWRecompAndroid#credits).
