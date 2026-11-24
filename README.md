# FNaF World no PS4 — runtime nativo (Etapa 1: leitor básico)

Objetivo do projeto: em vez de recriar o FNaF World do zero, criar um **runtime/interpretador nativo para PS4**
(OpenOrbis + C++) que lê os **assets extraídos do jogo original** (sprites, sons, frames, eventos) e desenha por cima
um **overlay com os botões do DualShock 4** (X, Quadrado, Triângulo, Círculo) nas áreas clicáveis, no estilo das
coletâneas da Capcom.

Tudo é compilado **na nuvem pelo GitHub Actions** — não precisa de PC.

## O que a Etapa 1 faz

1. **Workflow 1 (Windows)**: pega o `FNaFWorld.exe`, extrai as imagens com o **CTFAK 2.0** (extrator de Clickteam Fusion 2.5),
   comandado por scripts **Python**, e converte tudo para um formato simples para o PS4.
2. **Workflow 2 (Linux)**: baixa o **OpenOrbis PS4 Toolchain**, compila o leitor em C++ e gera o **`.pkg`** com os assets dentro.
3. **No PS4**: o app abre, mostra a imagem da tela inicial (você navega entre as imagens candidatas com o D-pad)
   e desenha os ícones dos botões do DualShock 4 sobre as áreas clicáveis. Ao apertar X / O / □ / △ o ícone correspondente acende.

> **Sobre "ferramenta Python":** o único extrator de Clickteam em Python (Anaconda) é Python 2.7, está abandonado,
> só gera `.mfa` (projeto do editor, não imagens) e falha em builds novos do Fusion.
> O extrator que funciona bem hoje é o **CTFAK 2.0** (C#/.NET 6, roda no runner Windows do GitHub).
> Os scripts em `tools/` (Python 3) fazem toda a orquestração: baixam/preparam o CTFAK, rodam, juntam as imagens,
> convertem para o formato do PS4 e geram relatórios.

## Passo a passo (só com o celular)

Veja também `COMO_INSTALAR.md` para colocar estes arquivos no repositório pelo celular.

1. **Mantenha o repositório PRIVADO.** Ele vai tratar de arquivos do jogo, que têm direitos autorais.
2. **Teste o pipeline do PS4 primeiro (sem o jogo):** aba **Actions → "2 - Compilar PS4 (PKG)" → Run workflow**.
   Sem assets extraídos, ele usa uma imagem de teste ("modo de teste"). Se isso gerar o `.pkg`, a parte de compilação está ok.
3. **Suba o `FNaFWorld.exe`** (o GitHub limita upload pelo navegador a 25 MB, então use uma Release):
   *Releases → Draft a new release* → tag **`assets`** → anexe o `FNaFWorld.exe` (ou um `.zip` com a pasta do jogo) → *Publish*.
4. **Extraia:** **Actions → "1 - Extrair assets (Windows)" → Run workflow.**
   (Opcional: em vez da Release, cole um link direto de download no campo `exe_url`.)
5. Quando o workflow 1 terminar com sucesso, o **workflow 2 roda sozinho**. No fim da página do run há o artefato
   **`fnafworld-ps4-pkg`** (zip com o `.pkg`).
6. Instale o `.pkg` no PS4 do jeito que você já instala homebrew.

### O que você vai ver no PS4

- Imagem de fundo (1920×1080, escalada a partir do tamanho do jogo) com um painel de informações no canto.
- Retângulos coloridos com o ícone do botão do DualShock 4 (por enquanto são áreas **de exemplo**, em `config/hotspots_title.txt`).

| Controle | Ação |
|---|---|
| D-pad ← → | imagem anterior / próxima (segure para repetir) |
| L1 / R1 | volta / avança 10 imagens |
| OPTIONS | liga/desliga o overlay |
| X, O, □, △ | acende os ícones/áreas desse botão e mostra "Acionado: ..." |

**Dica:** navegue até achar a tela inicial de verdade e **anote o número da imagem** (aparece no painel: `Imagem 7/40 img/0006.fwi`,
e o nome original do arquivo vem logo abaixo). Me mande esse número/nome e a foto da TV — com isso defino a camada certa e as áreas dos botões.

## Se algo der errado — o que me mandar

- **Workflow vermelho:** foto/print do passo que falhou (ou do resumo no fim da página do run).
  Os artefatos `relatorio-extracao` (workflow 1) e `logs-build-ps4` (workflow 2) têm os logs completos.
- **O app abre mas a tela fica preta/travada:** foto da TV. O app também grava `/data/fnafworld/log.txt` no PS4
  (acessível por FTP) com cada passo da inicialização.
- **Poucas/nenhuma imagem extraída:** o relatório mostra a versão/build do Clickteam do seu `.exe` (`inspect.txt`) e as
  tentativas do extrator. Com isso eu ajusto o extrator.

## Estrutura do repositório

```
.github/workflows/
  1-extract.yml        Windows: obtém o .exe -> CTFAK 2.0 -> imagens -> assets do PS4 (artefato ps4-assets)
  2-build-ps4.yml      Linux: toolchain OpenOrbis -> compila -> .pkg (artefato fnafworld-ps4-pkg)
tools/                 Python 3
  fetch_input.py       acha/baixa o FNaFWorld.exe (repositório, Release "assets" ou link)
  inspect_exe.py       diagnóstico: versão/build do Clickteam dentro do .exe
  extract_assets.py    prepara e roda o CTFAK 2.0 (tenta várias ferramentas/parâmetros)
  pack_assets.py       imagens -> formato FWI1 + manifest + frame + folhas de contato
  make_summary.py      resumo legível no celular (página do run)
  gen_font.py          (só para desenvolvimento) gera a fonte embutida
config/
  stage1.cfg           quantas candidatas, tamanho da janela, preferências
  hotspots_title.txt   áreas clicáveis da tela inicial + botão do DualShock 4 de cada uma
ps4/                   app do PS4 (C++)
  Makefile             build OpenOrbis (clang 18 + lld)
  sce_sys/icon0.png    ícone do app
  src/
    main.cpp                     ponto de entrada
    core/                        núcleo portátil (sem nada de PS4)
      app.*        lógica da Etapa 1 (navegação, overlay, HUD)
      frame.*      "frame" = camadas + áreas clicáveis (hotspots)
      manifest.*   lista de assets extraídos
      fwi.*        decodificador de imagem FWI1 (RGBA + RLE)
      surface.*    desenho em software (blend, escala bilinear, retângulos)
      padicons.*   ícones X / □ / △ / O desenhados proceduralmente
      text.*       texto com fonte bitmap embutida (font_data.h)
      arena.h, fileio.*, textparse.*, types.h
    platform/
      platform.h          interface (vídeo, controle, memória, log)
      ps4/platform_ps4.cpp   implementação PS4: sceVideoOut + ScePad
      host/platform_host.cpp implementação de teste no PC
ci/                    ajuda para o PkgTool.Core (container com libssl1.1, só se precisar)
tests/                 teste do núcleo no PC (g++ com sanitizers): bash tests/run_host_test.sh
```

## Arquitetura (módulos)

```
 FNaFWorld.exe ──(CTFAK 2.0, Python orquestra)──> imagens PNG
                                   │
                         tools/pack_assets.py
                                   ▼
        assets/manifest.txt   assets/img/*.fwi   assets/frames/title.frame
                                   │   (dentro do PKG: /app0/assets/)
                                   ▼
   core::App ──usa──> Manifest, Frame, FWI, Surface, Text, PadIcons
        │ (só fala com a plataforma por platform.h)
        ▼
   platform::ps4  (sceVideoOut, ScePad)      platform::host (testes no PC)
```

O núcleo não conhece o PS4: por isso dá para testar no PC e, nas próximas etapas, trocar ou acrescentar módulos
(áudio, eventos, máquina de estados dos frames) sem mexer na parte de vídeo/controle.

### Formatos

`manifest.txt`
```
fwmanifest 1
window 1280 720
placeholder 0
image 0 img/0000.fwi 1280 720 nome_original.png
title 0
frame title frames/title.frame
```

`frames/title.frame` (coordenadas no espaço da janela do jogo)
```
frame title 1280 720
layer title 0 0              # "title" = imagem selecionada no leitor; ou um número de imagem
hotspot Jogar 80 560 250 90 cross
```

`*.fwi` — cabeçalho de 20 bytes (`FWI1`, largura, altura, flags, tamanho) + pixels 0xAARRGGBB, com RLE opcional (`ps4/src/core/fwi.h`).

## Estado dos testes (seja realista)

- **Testado de verdade (no ambiente de desenvolvimento):** núcleo C++ no PC com AddressSanitizer/UBSan, conversor Python
  (ida e volta do formato FWI1), orquestrador do extrator com um CTFAK falso, parser do diagnóstico, validade dos YAMLs
  e expansão do Makefile.
- **Ainda NÃO testado (sem internet nem PS4 no ambiente onde escrevi):** compilar o CTFAK 2.0 no runner, rodá-lo no seu
  `.exe`, linkar contra o OpenOrbis v0.5.4 e rodar no console. Cada um desses pontos tem log detalhado para a gente
  corrigir rápido na primeira execução.

## Próximas etapas (ideias)

1. Fixar a imagem certa da tela inicial e as áreas reais dos botões (com sua foto/numeração).
2. Ler os **frames de verdade** (posições dos objetos) em vez de imagens soltas.
3. Sons/músicas (SDL2 áudio ou `sceAudioOut`).
4. Interpretador de eventos (ações/condições) para o jogo funcionar.
5. Navegação por cursor/ botões em todas as telas, com overlay por frame.
