/*
 * cutscene-maker runtime: boots into a configured stage, installs the generated
 * cutscene script, triggers it, and reports progress through PRINTF markers and
 * gCsmStatus. See the cutscene-maker repo, docs/architecture.md.
 */
#include "cutscene_maker.h"
#include "array_count.h"
#include "printf.h"
#include "actor.h"
#include "inventory.h"
#include "item.h"
#include "object.h"
#include "player.h"
#include "play_state.h"
#include "save.h"
#include "scene.h"
#include "sram.h"
#include "transition.h"

s32 Object_SpawnPersistent(ObjectContext* objectCtx, s16 objectId);
void Camera_RotateAroundPoint(PosRot* at, Vec3f* pos, Vec3f* dst);

CsmStatus gCsmStatus;
static u16 sCsmStartWait;
static u8 sCsmObjectError;
static u16 sCsmObjectErrorDetail;
static u16 sCsmLastPrintedFrame;

static void Csm_SetError(u8 code, u16 detail) {
    gCsmStatus.state = CSM_STATE_ERROR;
    gCsmStatus.error = code;
    gCsmStatus.errorDetail = detail;
    PRINTF("[CSM] ERROR %d %d\n", code, detail);
}

void Csm_BootSetup(struct GameState* gameState) {
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.save.linkAge = gCsmConfig.linkAge;
    Sram_InitDebugSave();
    gSaveContext.save.entranceIndex = gCsmConfig.entranceIndex;
    gSaveContext.save.dayTime = gCsmConfig.dayTime;
    gSaveContext.skyboxTime = gCsmConfig.dayTime;
    if (gCsmConfig.layerMode == CSM_LAYER_CUTSCENE) {
        gSaveContext.save.cutsceneIndex = CS_INDEX_0 + gCsmConfig.csLayer;
    } else {
        gSaveContext.save.cutsceneIndex = CS_INDEX_NONE;
    }
    gSaveContext.nextCutsceneIndex = NEXT_CS_INDEX_NONE;
    gSaveContext.cutsceneTrigger = 0;
    gSaveContext.respawnFlag = 0;
    if (gCsmConfig.tunic != 0) {
        Inventory_ChangeEquipment(EQUIP_TYPE_TUNIC, gCsmConfig.tunic);
    }
    PRINTF("[CSM] BOOT entrance=%d age=%d time=%04x layer=%d cs=%d\n", gCsmConfig.entranceIndex, gCsmConfig.linkAge,
           gCsmConfig.dayTime, gCsmConfig.layerMode, gCsmConfig.csLayer);
    SET_NEXT_GAMESTATE(gameState, Play_Init, PlayState);
}

void Csm_OnSceneInit(struct PlayState* play) {
    ObjectContext* objectCtx = &play->objectCtx;
    s32 i;

    sCsmObjectError = 0;
    if (gCsmConfig.layerMode != CSM_LAYER_NORMAL) {
        return;
    }
    for (i = 0; i < gCsmConfig.objectCount; i++) {
        s16 objectId = gCsmConfig.objects[i];
        u32 size = gObjectTable[objectId].vromEnd - gObjectTable[objectId].vromStart;

        if ((objectCtx->numEntries >= ARRAY_COUNT(objectCtx->slots)) ||
            (((uintptr_t)objectCtx->slots[objectCtx->numEntries].segment + size) >= (uintptr_t)objectCtx->spaceEnd)) {
            PRINTF("[CSM] object %d does not fit (entries=%d)\n", objectId, objectCtx->numEntries);
            sCsmObjectError = 1;
            sCsmObjectErrorDetail = objectId;
            continue;
        }
        Object_SpawnPersistent(objectCtx, objectId);
        PRINTF("[CSM] object %d loaded\n", objectId);
    }
}

void Csm_OnPlayInit(struct PlayState* play) {
    gCsmStatus.state = CSM_STATE_IDLE;
    gCsmStatus.error = 0;
    gCsmStatus.errorDetail = 0;
    gCsmStatus.csFrame = 0;
    gCsmStatus.playFrames = 0;
    sCsmStartWait = 0;
    sCsmLastPrintedFrame = 0xFFFF;
    gSaveContext.cutsceneTrigger = 0;

    if (sCsmObjectError) {
        Csm_SetError(CSM_ERR_OBJECT_SPACE, sCsmObjectErrorDetail);
        return;
    }
    if (gCsmConfig.layerMode == CSM_LAYER_CUTSCENE) {
        play->csCtx.script = (void*)gCsmScript;
        gCsmStatus.state = CSM_STATE_STARTING;
    } else {
        gCsmStatus.state = CSM_STATE_SETTLING;
    }
    PRINTF("[CSM] PLAYINIT sceneLayer=%d state=%d\n", gSaveContext.sceneLayer, gCsmStatus.state);
}

static void Csm_SpawnActorsAndTrigger(struct PlayState* play) {
    Player* player = GET_PLAYER(play);
    PosRot playerPosRot;
    s32 i;

    // Same reference the camera's relative mode uses (Camera_Demo1 -> Actor_GetWorld).
    playerPosRot = Actor_GetWorld(&player->actor);
    PRINTF("[CSM] PLAYER %d %d %d yaw=%d\n", (s32)player->actor.world.pos.x, (s32)player->actor.world.pos.y,
           (s32)player->actor.world.pos.z, playerPosRot.rot.y);

    for (i = 0; i < gCsmConfig.actorCount; i++) {
        const CsmActorSpawn* spawn = &gCsmConfig.actors[i];
        Vec3f pos = spawn->pos;
        s16 yaw = spawn->rot.y;
        Actor* actor;

        if (spawn->relative) {
            Vec3f rel = spawn->pos;

            Camera_RotateAroundPoint(&playerPosRot, &rel, &pos);
            yaw += playerPosRot.rot.y;
        }
        actor = Actor_Spawn(&play->actorCtx, play, spawn->actorId, pos.x, pos.y, pos.z, spawn->rot.x, yaw,
                            spawn->rot.z, spawn->params);
        if (actor == NULL) {
            Csm_SetError(CSM_ERR_ACTOR_SPAWN, i);
            return;
        }
        PRINTF("[CSM] actor %d spawned at %d %d %d\n", spawn->actorId, (s32)pos.x, (s32)pos.y, (s32)pos.z);
    }

    play->csCtx.script = (void*)gCsmScript;
    gSaveContext.cutsceneTrigger = 1;
    sCsmStartWait = 0;
    gCsmStatus.state = CSM_STATE_STARTING;
    PRINTF("[CSM] TRIGGER playFrames=%d\n", gCsmStatus.playFrames);
}

void Csm_Update(struct PlayState* play) {
    CutsceneContext* csCtx = &play->csCtx;

    gCsmStatus.playFrames++;

    switch (gCsmStatus.state) {
        case CSM_STATE_SETTLING:
            if ((gCsmStatus.playFrames >= gCsmConfig.settleFrames) && (play->transitionMode == TRANS_MODE_OFF) &&
                (csCtx->state == CS_STATE_IDLE)) {
                Csm_SpawnActorsAndTrigger(play);
            }
            break;
        case CSM_STATE_STARTING:
            if (csCtx->state != CS_STATE_IDLE) {
                gCsmStatus.state = CSM_STATE_RUNNING;
                PRINTF("[CSM] START playFrames=%d\n", gCsmStatus.playFrames);
            } else if (++sCsmStartWait > 120) {
                Csm_SetError(CSM_ERR_NO_START, csCtx->state);
            }
            break;
        case CSM_STATE_RUNNING:
            gCsmStatus.csFrame = csCtx->curFrame;
            if (((csCtx->curFrame % 20) == 0) && (csCtx->curFrame != sCsmLastPrintedFrame)) {
                sCsmLastPrintedFrame = csCtx->curFrame;
                PRINTF("[CSM] F %d\n", csCtx->curFrame);
            }
            if (csCtx->state == CS_STATE_IDLE) {
                gCsmStatus.state = CSM_STATE_ENDED;
                PRINTF("[CSM] END playFrames=%d\n", gCsmStatus.playFrames);
            }
            break;
        default:
            break;
    }
}
