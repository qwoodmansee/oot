#include "console_logo_state.h"
#include "setup_state.h"
#include "printf.h"
#include "translation.h"
#include "save.h"
#if CUTSCENE_MAKER
#include "cutscene_maker.h"
#endif

void Setup_InitImpl(SetupState* this) {
    PRINTF(T("ゼルダ共通データ初期化\n", "Zelda common data initialization\n"));
    SaveContext_Init();
    this->state.running = false;
#if CUTSCENE_MAKER
    Csm_BootSetup(&this->state);
#else
    SET_NEXT_GAMESTATE(&this->state, ConsoleLogo_Init, ConsoleLogoState);
#endif
}

void Setup_Destroy(GameState* thisx) {
}

void Setup_Init(GameState* thisx) {
    SetupState* this = (SetupState*)thisx;

    this->state.destroy = Setup_Destroy;
    Setup_InitImpl(this);
}
