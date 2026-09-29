#ifndef CUTSCENE_MAKER_H
#define CUTSCENE_MAKER_H

#include "ultra64.h"
#include "z_math.h"
#include "cutscene.h"

struct GameState;
struct PlayState;

#define CSM_LAYER_NORMAL 0
#define CSM_LAYER_CUTSCENE 1

typedef enum CsmState {
    /* 0 */ CSM_STATE_IDLE,
    /* 1 */ CSM_STATE_SETTLING,
    /* 2 */ CSM_STATE_STARTING,
    /* 3 */ CSM_STATE_RUNNING,
    /* 4 */ CSM_STATE_ENDED,
    /* 5 */ CSM_STATE_ERROR
} CsmState;

#define CSM_ERR_OBJECT_SPACE 1
#define CSM_ERR_ACTOR_SPAWN 2
#define CSM_ERR_NO_START 3

typedef struct CsmActorSpawn {
    /* 0x00 */ s16 actorId;
    /* 0x02 */ s16 params;
    /* 0x04 */ Vec3f pos;      // world units, or relative to Link when `relative` is set
    /* 0x10 */ Vec3s rot;      // binang; yaw is added to Link's yaw when `relative` is set
    /* 0x16 */ u8 relative;    // 1: pos and yaw are relative to Link (Z forward, X right, Y up)
} CsmActorSpawn;

typedef struct CsmConfig {
    /* 0x00 */ u16 entranceIndex;
    /* 0x02 */ u8 linkAge;      // LINK_AGE_ADULT or LINK_AGE_CHILD
    /* 0x04 */ u16 dayTime;     // CLOCK_TIME(h, m)
    /* 0x06 */ u8 layerMode;    // CSM_LAYER_NORMAL or CSM_LAYER_CUTSCENE
    /* 0x07 */ u8 csLayer;      // 0..9 when layerMode == CSM_LAYER_CUTSCENE
    /* 0x08 */ u16 settleFrames;
    /* 0x0A */ u8 objectCount;
    /* 0x0B */ u8 actorCount;
    /* 0x0C */ const s16* objects;
    /* 0x10 */ const CsmActorSpawn* actors;
    /* 0x14 */ u16 scriptFrameCount;
    /* 0x16 */ u8 tunic;        // 0 keeps the save's tunic, else EQUIP_VALUE_TUNIC_*
    /* 0x17 */ u8 removedCount;      // actor ids to kill every frame while the runtime is active
    /* 0x18 */ const s16* removedIds; // (Navi hint triggers, bosses whose intro would take the camera, ...)
} CsmConfig;

// Read by the capture harness through the linker map. Keep the layout stable.
typedef struct CsmStatus {
    /* 0x00 */ u8 state;        // CsmState
    /* 0x01 */ u8 error;        // CSM_ERR_*
    /* 0x02 */ u16 csFrame;     // csCtx.curFrame while running
    /* 0x04 */ u32 playFrames;  // Play_Update calls since Play_Init
    /* 0x08 */ u16 errorDetail;
} CsmStatus;

extern const CsmConfig gCsmConfig;
extern const CutsceneData gCsmScript[];
extern CsmStatus gCsmStatus;

void Csm_BootSetup(struct GameState* gameState);
void Csm_OnSceneInit(struct PlayState* play);
void Csm_OnPlayInit(struct PlayState* play);
void Csm_Update(struct PlayState* play);

#endif
