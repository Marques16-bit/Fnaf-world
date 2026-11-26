// Aplicativo da Etapa 1: leitor basico.
//  - carrega o manifesto + frame "title" dos assets extraidos
//  - mostra a imagem de titulo (navegavel entre candidatos com D-pad / L1 / R1)
//  - desenha o overlay de botoes do DualShock 4 sobre as areas clicaveis
#pragma once
#include "arena.h"
#include "frame.h"
#include "manifest.h"
#include "surface.h"
#include "../platform/platform.h"

struct App {
    int fbw;
    int fbh;

    Manifest man;
    Frame frame;
    bool man_ok;
    bool frame_ok;
    char status[200];  // ultima mensagem de erro/aviso

    Surface scene;     // cena pre-composta (fundo), do tamanho do framebuffer
    Surface tmp;       // buffer de decodificacao de imagens
    uint8_t* filebuf;  // buffer de leitura de arquivos
    size_t filecap;

    int sel;           // candidato exibido
    bool overlay;
    bool need_compose;

    uint32_t prev_buttons;
    int hold_left, hold_right;
    int flash[FRAME_MAX_HOTSPOTS];
    char last_action[96];
    int last_action_timer;
    uint32_t frame_no;
    bool pad_ok;
    bool first_frame;   // forca o 1o desenho
    bool redraw;        // algo mudou: precisa redesenhar e enviar um quadro novo

    // transformacao jogo -> tela
    float scale;
    int off_x;
    int off_y;
};

// Inicializa (aloca da arena, le arquivos, compoe a cena). Sempre deixa o app em estado desenhavel.
void app_init(App* app, Arena* arena, int fbw, int fbh);
// Atualiza o estado. Retorna true se a tela mudou e precisa ser redesenhada (senao o quadro anterior continua valendo).
bool app_update(App* app, const plat::PadState* pad);
void app_draw(App* app, Surface* fb);
