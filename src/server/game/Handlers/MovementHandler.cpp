/*
 * Copyright (C) 2008-2017 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "Corpse.h"
#include "Language.h"
#include "Player.h"
#include "MapManager.h"
#include "Transport.h"
#include "Battleground.h"
#include "WaypointMovementGenerator.h"
#include "InstanceSaveMgr.h"
#include "ObjectMgr.h"
#include "Vehicle.h"
#include "MovementPacketSender.h"

void WorldSession::HandleMoveWorldportAckOpcode(WorldPacket & /*recvData*/)
{
    TC_LOG_DEBUG("network", "WORLD: got MSG_MOVE_WORLDPORT_ACK.");
    HandleMoveWorldportAck();
}

void WorldSession::HandleMoveWorldportAck()
{
    // ignore unexpected far teleports
    if (!GetPlayer()->IsBeingTeleportedFar())
        return;

    GetPlayer()->SetSemaphoreTeleportFar(false);

    // get the teleport destination
    WorldLocation const& loc = GetPlayer()->GetTeleportDest();

    // possible errors in the coordinate validity check
    if (!MapManager::IsValidMapCoord(loc))
    {
        LogoutPlayer(false);
        return;
    }

    // get the destination map entry, not the current one, this will fix homebind and reset greeting
    MapEntry const* mEntry = sMapStore.LookupEntry(loc.GetMapId());
    InstanceTemplate const* mInstance = sObjectMgr->GetInstanceTemplate(loc.GetMapId());

    // reset instance validity, except if going to an instance inside an instance
    if (GetPlayer()->m_InstanceValid == false && !mInstance)
        GetPlayer()->m_InstanceValid = true;

    Map* oldMap = GetPlayer()->GetMap();
    Map* newMap = sMapMgr->CreateMap(loc.GetMapId(), GetPlayer());

    if (GetPlayer()->IsInWorld())
    {
        TC_LOG_ERROR("network", "%s %s is still in world when teleported from map %s (%u) to new map %s (%u)", GetPlayer()->GetGUID().ToString().c_str(), GetPlayer()->GetName().c_str(), oldMap->GetMapName(), oldMap->GetId(), newMap ? newMap->GetMapName() : "Unknown", loc.GetMapId());
        oldMap->RemovePlayerFromMap(GetPlayer(), false);
    }

    // relocate the player to the teleport destination
    // the CannotEnter checks are done in TeleporTo but conditions may change
    // while the player is in transit, for example the map may get full
    if (!newMap || newMap->CannotEnter(GetPlayer()))
    {
        TC_LOG_ERROR("network", "Map %d (%s) could not be created for player %d (%s), porting player to homebind", loc.GetMapId(), newMap ? newMap->GetMapName() : "Unknown", GetPlayer()->GetGUID().GetCounter(), GetPlayer()->GetName().c_str());
        GetPlayer()->TeleportTo(GetPlayer()->m_homebindMapId, GetPlayer()->m_homebindX, GetPlayer()->m_homebindY, GetPlayer()->m_homebindZ, GetPlayer()->GetOrientation());
        return;
    }

    float z = loc.GetPositionZ();
    if (GetPlayer()->HasUnitMovementFlag(MOVEMENTFLAG_HOVER))
        z += GetPlayer()->GetFloatValue(UNIT_FIELD_HOVERHEIGHT);
    GetPlayer()->Relocate(loc.GetPositionX(), loc.GetPositionY(), z, loc.GetOrientation());

    GetPlayer()->ResetMap();
    GetPlayer()->SetMap(newMap);

    GetPlayer()->SendInitialPacketsBeforeAddToMap();
    if (!GetPlayer()->GetMap()->AddPlayerToMap(GetPlayer()))
    {
        TC_LOG_ERROR("network", "WORLD: failed to teleport player %s (%d) to map %d (%s) because of unknown reason!",
            GetPlayer()->GetName().c_str(), GetPlayer()->GetGUID().GetCounter(), loc.GetMapId(), newMap ? newMap->GetMapName() : "Unknown");
        GetPlayer()->ResetMap();
        GetPlayer()->SetMap(oldMap);
        GetPlayer()->TeleportTo(GetPlayer()->m_homebindMapId, GetPlayer()->m_homebindX, GetPlayer()->m_homebindY, GetPlayer()->m_homebindZ, GetPlayer()->GetOrientation());
        return;
    }

    // battleground state prepare (in case join to BG), at relogin/tele player not invited
    // only add to bg group and object, if the player was invited (else he entered through command)
    if (_player->InBattleground())
    {
        // cleanup setting if outdated
        if (!mEntry->IsBattlegroundOrArena())
        {
            // We're not in BG
            _player->SetBattlegroundId(0, BATTLEGROUND_TYPE_NONE);
            // reset destination bg team
            _player->SetBGTeam(0);
        }
        // join to bg case
        else if (Battleground* bg = _player->GetBattleground())
        {
            if (_player->IsInvitedForBattlegroundInstance(_player->GetBattlegroundId()))
                bg->AddPlayer(_player);
        }
    }

    GetPlayer()->SendInitialPacketsAfterAddToMap();

    // flight fast teleport case
    if (GetPlayer()->GetMotionMaster()->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE)
    {
        if (!_player->InBattleground())
        {
            // short preparations to continue flight
            FlightPathMovementGenerator* flight = (FlightPathMovementGenerator*)(GetPlayer()->GetMotionMaster()->top());
            flight->Initialize(GetPlayer());
            return;
        }

        // battleground state prepare, stop flight
        GetPlayer()->GetMotionMaster()->MovementExpired();
        GetPlayer()->CleanupAfterTaxiFlight();
    }

    // resurrect character at enter into instance where his corpse exist after add to map

    if (mEntry->IsDungeon() && !GetPlayer()->IsAlive())
        if (GetPlayer()->GetCorpseLocation().GetMapId() == mEntry->MapID)
        {
            GetPlayer()->ResurrectPlayer(0.5f, false);
            GetPlayer()->SpawnCorpseBones();
        }

    bool allowMount = !mEntry->IsDungeon() || mEntry->IsBattlegroundOrArena();
    if (mInstance)
    {
        // check if this instance has a reset time and send it to player if so
        Difficulty diff = GetPlayer()->GetDifficulty(mEntry->IsRaid());
        if (MapDifficulty const* mapDiff = GetMapDifficultyData(mEntry->MapID, diff))
        {
            if (mapDiff->resetTime)
            {
                if (time_t timeReset = sInstanceSaveMgr->GetResetTimeFor(mEntry->MapID, diff))
                {
                    uint32 timeleft = uint32(timeReset - time(NULL));
                    GetPlayer()->SendInstanceResetWarning(mEntry->MapID, diff, timeleft, true);
                }
            }
        }

        // check if instance is valid
        if (!GetPlayer()->CheckInstanceValidity(false))
            GetPlayer()->m_InstanceValid = false;

        // instance mounting is handled in InstanceTemplate
        allowMount = mInstance->AllowMount;
    }

    // mount allow check
    if (!allowMount)
        _player->RemoveAurasByType(SPELL_AURA_MOUNTED);

    // update zone immediately, otherwise leave channel will cause crash in mtmap
    uint32 newzone, newarea;
    GetPlayer()->GetZoneAndAreaId(newzone, newarea);
    GetPlayer()->UpdateZone(newzone, newarea);

    // honorless target
    if (GetPlayer()->pvpInfo.IsHostile)
        GetPlayer()->CastSpell(GetPlayer(), 2479, true);

    // in friendly area
    else if (GetPlayer()->IsPvP() && !GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP))
        GetPlayer()->UpdatePvP(false, false);

    // resummon pet
    GetPlayer()->ResummonPetTemporaryUnSummonedIfAny();

    //lets process all delayed operations on successful teleport
    GetPlayer()->ProcessDelayedOperations();
}

void WorldSession::HandleMoveTeleportAck(WorldPacket& recvData)
{
    TC_LOG_DEBUG("network", "MSG_MOVE_TELEPORT_ACK");
    ObjectGuid guid;

    recvData >> guid.ReadAsPacked();

    uint32 flags, time;
    recvData >> flags >> time;

    Player* plMover = _player->m_unitMovedByMe->ToPlayer();

    if (!plMover || !plMover->IsBeingTeleportedNear())
        return;

    if (guid != plMover->GetGUID())
        return;

    plMover->SetSemaphoreTeleportNear(false);

    uint32 old_zone = plMover->GetZoneId();

    WorldLocation const& dest = plMover->GetTeleportDest();

    plMover->UpdatePosition(dest, true);

    uint32 newzone, newarea;
    plMover->GetZoneAndAreaId(newzone, newarea);
    plMover->UpdateZone(newzone, newarea);

    // new zone
    if (old_zone != newzone)
    {
        // honorless target
        if (plMover->pvpInfo.IsHostile)
            plMover->CastSpell(plMover, 2479, true);

        // in friendly area
        else if (plMover->IsPvP() && !plMover->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP))
            plMover->UpdatePvP(false, false);
    }

    // resummon pet
    GetPlayer()->ResummonPetTemporaryUnSummonedIfAny();

    //lets process all delayed operations on successful teleport
    GetPlayer()->ProcessDelayedOperations();
}

/*
Handles:
MSG_MOVE_START_FORWARD
MSG_MOVE_START_BACKWARD
MSG_MOVE_STOP
MSG_MOVE_START_STRAFE_LEFT
MSG_MOVE_START_STRAFE_RIGHT
MSG_MOVE_STOP_STRAFE
MSG_MOVE_JUMP
MSG_MOVE_START_TURN_LEFT
MSG_MOVE_START_TURN_RIGHT
MSG_MOVE_STOP_TURN

MSG_MOVE_START_PITCH_UP
MSG_MOVE_START_PITCH_DOWN
MSG_MOVE_STOP_PITCH
MSG_MOVE_SET_RUN_MODE
MSG_MOVE_SET_WALK_MODE
MSG_MOVE_FALL_LAND
MSG_MOVE_START_SWIM
MSG_MOVE_STOP_SWIM
MSG_MOVE_SET_FACING
MSG_MOVE_SET_PITCH

MSG_MOVE_HEARTBEAT
CMSG_MOVE_FALL_RESET (?)
CMSG_MOVE_SET_FLY (?)
MSG_MOVE_START_ASCEND
MSG_MOVE_STOP_ASCEND
CMSG_MOVE_CHNG_TRANSPORT (?)
MSG_MOVE_START_DESCEND
*/
void WorldSession::HandleMovementOpcodes(WorldPacket& recvData)
{
    uint16 opcode = recvData.GetOpcode();
    Unit* mover = _player->GetUnitBeingMoved();
    Player* plrMover = mover->ToPlayer();

    ASSERT(mover != NULL);                      // there must always be a mover

    /* extract packet */
    MovementInfo movementInfo;
    movementInfo.FillContentFromPacket(&recvData, true);
    recvData.rfinish();                         // prevent warnings spam

    // ignore, waiting processing in WorldSession::HandleMoveWorldportAckOpcode and WorldSession::HandleMoveTeleportAck
    if (plrMover && plrMover->IsBeingTeleported())
    {
        recvData.rfinish();                     // prevent warnings spam
        return;
    }

    // prevent tampered movement data
    if (movementInfo.guid != mover->GetGUID())
        return;

    if (!movementInfo.pos.IsPositionValid())
        return;

    /* validate (and correct if necessary) new movement packet */
    mover->ValidateNewMovementInfo(&movementInfo);

    /* handle special cases */
    if (movementInfo.HasMovementFlag(MOVEMENTFLAG_ONTRANSPORT)) // @todo: move this stuff. CMSG_MOVE_CHNG_TRANSPORT should be handled elsewhere than here.
    {
        // transports size limited
        // (also received at zeppelin leave by some reason with t_* as absolute in continent coordinates, can be safely skipped)
        if (movementInfo.transport.pos.GetPositionX() > 50 || movementInfo.transport.pos.GetPositionY() > 50 || movementInfo.transport.pos.GetPositionZ() > 50)
            return;

        if (!Trinity::IsValidMapCoord(movementInfo.pos.GetPositionX() + movementInfo.transport.pos.GetPositionX(), movementInfo.pos.GetPositionY() + movementInfo.transport.pos.GetPositionY(),
            movementInfo.pos.GetPositionZ() + movementInfo.transport.pos.GetPositionZ(), movementInfo.pos.GetOrientation() + movementInfo.transport.pos.GetOrientation()))
            return;

        // if we boarded a transport, add us to it
        if (plrMover)
        {
            if (!plrMover->GetTransport())
            {
                if (Transport* transport = plrMover->GetMap()->GetTransport(movementInfo.transport.guid))
                    transport->AddPassenger(plrMover);
            }
            else if (plrMover->GetTransport()->GetGUID() != movementInfo.transport.guid)
            {
                plrMover->GetTransport()->RemovePassenger(plrMover);
                if (Transport* transport = plrMover->GetMap()->GetTransport(movementInfo.transport.guid))
                    transport->AddPassenger(plrMover);
                else
                    movementInfo.transport.Reset();
            }
        }

        if (!mover->GetTransport() && !mover->GetVehicle())
        {
            GameObject* go = mover->GetMap()->GetGameObject(movementInfo.transport.guid);
            if (!go || go->GetGoType() != GAMEOBJECT_TYPE_TRANSPORT)
                movementInfo.RemoveMovementFlag(MOVEMENTFLAG_ONTRANSPORT);
        }
    }
    else if (plrMover && plrMover->GetTransport())                // if we were on a transport, leave
    {
        plrMover->GetTransport()->RemovePassenger(plrMover);
        movementInfo.transport.Reset();
    }

    // fall damage generation (ignore in flight case that can be triggered also at lags in moment teleportation to another map).
    if (opcode == MSG_MOVE_FALL_LAND && plrMover && !plrMover->IsInFlight())
        plrMover->HandleFall(movementInfo);

    if (plrMover && ((movementInfo.flags & MOVEMENTFLAG_SWIMMING) != 0) != plrMover->IsInWater())
    {
        // now client not include swimming flag in case jumping under water
        plrMover->SetInWater(!plrMover->IsInWater() || plrMover->GetBaseMap()->IsUnderWater(movementInfo.pos.GetPositionX(), movementInfo.pos.GetPositionY(), movementInfo.pos.GetPositionZ()));
    }

    /* process position-change */
    mover->UpdateMovementInfo(movementInfo);

    WorldPacket data(opcode, recvData.size());
    mover->GetMovementInfo().WriteContentIntoPacket(&data, true);
    mover->SendMessageToSet(&data, _player);

    // Some vehicles allow the passenger to turn by himself
    if (Vehicle* vehicle = mover->GetVehicle())
    {
        if (VehicleSeatEntry const* seat = vehicle->GetSeatForPassenger(mover))
        {
            if (seat->m_flags & VEHICLE_SEAT_FLAG_ALLOW_TURNING)
            {
                if (movementInfo.pos.GetOrientation() != mover->GetOrientation())
                {
                    mover->SetOrientation(movementInfo.pos.GetOrientation());
                    mover->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TURNING);
                }
            }
        }
        return;
    }

    mover->UpdatePosition(movementInfo.pos); // unsure if this can be safely deleted since it is also called in "mover->UpdateMovementInfo(movementInfo)" but the above if blocks may influence the unit's orintation

    if (plrMover)                                            // nothing is charmed, or player charmed
    {
        if (plrMover->IsSitState() && (movementInfo.flags & (MOVEMENTFLAG_MASK_MOVING | MOVEMENTFLAG_MASK_TURNING)))
            plrMover->SetStandState(UNIT_STAND_STATE_STAND);

        plrMover->UpdateFallInformationIfNeed(movementInfo, opcode);

        if (movementInfo.pos.GetPositionZ() < plrMover->GetMap()->GetMinHeight(movementInfo.pos.GetPositionX(), movementInfo.pos.GetPositionY()))
        {
            if (!(plrMover->GetBattleground() && plrMover->GetBattleground()->HandlePlayerUnderMap(_player)))
            {
                // NOTE: this is actually called many times while falling
                // even after the player has been teleported away
                /// @todo discard movement packets after the player is rooted
                if (plrMover->IsAlive())
                {
                    plrMover->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_IS_OUT_OF_BOUNDS);
                    plrMover->EnvironmentalDamage(DAMAGE_FALL_TO_VOID, GetPlayer()->GetMaxHealth());
                    // player can be alive if GM/etc
                    // change the death state to CORPSE to prevent the death timer from
                    // starting in the next player update
                    if (plrMover->IsAlive())
                        plrMover->KillPlayer();
                }
            }
        }
    }
}

void WorldSession::HandleForceSpeedChangeAck(WorldPacket &recvData)
{
    Unit* mover = _player->GetUnitBeingMoved();
    ASSERT(mover);

    /* extract packet */
    ObjectGuid guid;
    recvData >> guid.ReadAsPacked();

    // now can skip not our packet
    if (mover->GetGUID() != guid)
    {
        recvData.rfinish();                   // prevent warnings spam
        return;
    }

    UnitMoveType move_type;
    switch (recvData.GetOpcode())
    {
        case CMSG_FORCE_WALK_SPEED_CHANGE_ACK:          move_type = MOVE_WALK;          break;
        case CMSG_FORCE_RUN_SPEED_CHANGE_ACK:           move_type = MOVE_RUN;           break;
        case CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK:      move_type = MOVE_RUN_BACK;      break;
        case CMSG_FORCE_SWIM_SPEED_CHANGE_ACK:          move_type = MOVE_SWIM;          break;
        case CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK:     move_type = MOVE_SWIM_BACK;     break;
        case CMSG_FORCE_TURN_RATE_CHANGE_ACK:           move_type = MOVE_TURN_RATE;     break;
        case CMSG_FORCE_FLIGHT_SPEED_CHANGE_ACK:        move_type = MOVE_FLIGHT;        break;
        case CMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK:   move_type = MOVE_FLIGHT_BACK;   break;
        case CMSG_FORCE_PITCH_RATE_CHANGE_ACK:          move_type = MOVE_PITCH_RATE;    break;
        default:
            TC_LOG_ERROR("network", "WorldSession::HandleForceSpeedChangeAck: Unknown move type opcode: %u", recvData.GetOpcode());
            return;
    }

    uint32 movementCounter;
    float  speedReceived;
    MovementInfo movementInfo;
    movementInfo.guid = guid;

    recvData >> movementCounter;
    movementInfo.FillContentFromPacket(&recvData);
    recvData >> speedReceived;

    TC_LOG_ERROR("custom", "PRE-VALIDATION received speed ack. movement counter: %u. new speed rate: %f", movementCounter, speedReceived);

    // verify that indeed the client is replying with the changes that were send to him
    if (!mover->HasPendingMovementChange())
    {
        TC_LOG_INFO("cheat", "WorldSession::HandleForceSpeedChangeAck: Player %s from account id %u kicked because no movement change ack was expected from this player",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();
        return;
    }

    PlayerMovementPendingChange pendingChange = mover->PopPendingMovementChange();
    float speedSent = pendingChange.newValue;
    MovementChangeType changeType = pendingChange.movementChangeType;
    UnitMoveType moveTypeSent;
    switch (changeType)
    {
        case SPEED_CHANGE_WALK:                 moveTypeSent = MOVE_WALK; break;
        case SPEED_CHANGE_RUN:                  moveTypeSent = MOVE_RUN; break;
        case SPEED_CHANGE_RUN_BACK:             moveTypeSent = MOVE_RUN_BACK; break;
        case SPEED_CHANGE_SWIM:                 moveTypeSent = MOVE_SWIM; break;
        case SPEED_CHANGE_SWIM_BACK:            moveTypeSent = MOVE_SWIM_BACK; break;
        case RATE_CHANGE_TURN:                  moveTypeSent = MOVE_TURN_RATE; break;
        case SPEED_CHANGE_FLIGHT_SPEED:         moveTypeSent = MOVE_FLIGHT; break;
        case SPEED_CHANGE_FLIGHT_BACK_SPEED:    moveTypeSent = MOVE_FLIGHT_BACK; break;
        case RATE_CHANGE_PITCH:                 moveTypeSent = MOVE_PITCH_RATE; break;
        default:
            TC_LOG_INFO("cheat", "WorldSession::HandleForceSpeedChangeAck: Player %s from account id %u kicked for incorrect data returned in an ack",
                _player->GetName().c_str(), _player->GetSession()->GetAccountId());
            _player->GetSession()->KickPlayer();
            return;
    }

    if (pendingChange.movementCounter != movementCounter || std::fabs(speedSent - speedReceived) > 0.01f || moveTypeSent!= move_type)
    {
        TC_LOG_INFO("cheat", "WorldSession::HandleForceSpeedChangeAck: Player %s from account id %u kicked for incorrect data returned in an ack",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();
        return;
    }

    /* the client data has been verified. let's do the actual change now */
    float newSpeedRate = speedSent / (mover->IsControlledByPlayer() ? playerBaseMoveSpeed[move_type] : baseMoveSpeed[move_type]); // is it sure that IsControlledByPlayer() should be used?
    TC_LOG_ERROR("custom", "POST-VALIDATION received speed ack. movement counter: %u. new speed rate: %f", movementCounter, speedReceived);
    mover->UpdateMovementInfo(movementInfo);
    mover->SetSpeedRateReal(move_type, newSpeedRate);
    MovementPacketSender::SendSpeedChangeToObservers(mover, move_type, newSpeedRate);
}

void WorldSession::HandleCollisionHeightChangeAck(WorldPacket &recvData)
{
    Unit* mover = _player->GetUnitBeingMoved();
    ASSERT(mover);

    /* extract packet */
    ObjectGuid guid;
    uint32 movementCounter;
    MovementInfo movementInfo;
    float  heightReceived;

    recvData >> guid.ReadAsPacked();
    movementInfo.guid = guid;
    recvData >> movementCounter;
    movementInfo.FillContentFromPacket(&recvData, false);
    recvData >> heightReceived;
    TC_LOG_ERROR("custom", "PRE-VALIDATION received height ack. movement counter: %u. new speed rate: %f", movementCounter, heightReceived);

    // now can skip not our packet
    if (mover->GetGUID() != movementInfo.guid) // @todo: potential hack attempt. use disciplinary measure?
        return;

    // verify that indeed the client is replying with the changes that were send to him
    if (!mover->HasPendingMovementChange())
    {
        TC_LOG_INFO("cheat", "WorldSession::HandleCollisionHeightChangeAck: Player %s from account id %u kicked because no movement change ack was expected from this player",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();
        return;
    }

    PlayerMovementPendingChange pendingChange = mover->PopPendingMovementChange();
    float heightSent = pendingChange.newValue;
    MovementChangeType changeType = pendingChange.movementChangeType;

    if (pendingChange.movementCounter != movementCounter || changeType != SET_COLLISION_HGT || std::fabs(heightSent - heightReceived) > 0.01f)
    {
        TC_LOG_INFO("cheat", "WorldSession::HandleCollisionHeightChangeAck: Player %s from account id %u kicked for incorrect data returned in an ack",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();
        return;
    }

    /* the client data has been verified. let's do the actual change now */
    TC_LOG_ERROR("custom", "POST-VALIDATION received height ack. movement counter: %u. new speed rate: %f", movementCounter, heightReceived);
    mover->UpdateMovementInfo(movementInfo);
    mover->SetCollisionHeightReal(heightSent);
    MovementPacketSender::SendHeightChangeToObservers(mover, heightSent);
}

void WorldSession::HandleSetActiveMoverOpcode(WorldPacket &recvData)
{
    TC_LOG_DEBUG("network", "WORLD: Recvd CMSG_SET_ACTIVE_MOVER");

    ObjectGuid guid;
    recvData >> guid;

    if (GetPlayer()->IsInWorld())
        if (_player->m_unitMovedByMe->GetGUID() != guid)
            TC_LOG_DEBUG("network", "HandleSetActiveMoverOpcode: incorrect mover guid: mover is %s and should be %s" , guid.ToString().c_str(), _player->m_unitMovedByMe->GetGUID().ToString().c_str());
}

void WorldSession::HandleMoveNotActiveMover(WorldPacket &recvData)
{
    TC_LOG_DEBUG("network", "WORLD: Recvd CMSG_MOVE_NOT_ACTIVE_MOVER");
    MovementInfo mi;
    mi.FillContentFromPacket(&recvData, true);
    _player->m_movementInfo = mi;
}

void WorldSession::HandleMountSpecialAnimOpcode(WorldPacket& /*recvData*/)
{
    WorldPacket data(SMSG_MOUNTSPECIAL_ANIM, 8);
    data << uint64(GetPlayer()->GetGUID());

    GetPlayer()->SendMessageToSet(&data, false);
}

void WorldSession::HandleMoveKnockBackAck(WorldPacket& recvData)
{
    Unit* mover = _player->GetUnitBeingMoved();
    ASSERT(mover);

    /* extract packet */
    ObjectGuid guid;
    uint32 movementCounter;
    MovementInfo movementInfo;

    recvData >> guid.ReadAsPacked();
    movementInfo.guid = guid;
    recvData >> movementCounter;
    movementInfo.FillContentFromPacket(&recvData, false);
    TC_LOG_ERROR("custom", "PRE-VALIDATION received knockback ack. movement counter: %u. vcos: %f, vsin: %f, speedXY: %f, speedZ: %f", movementInfo.guid, movementInfo.jump.cosAngle, movementInfo.jump.sinAngle, movementInfo.jump.xyspeed, movementInfo.jump.zspeed);

    // now can skip not our packet
    if (mover->GetGUID() != movementInfo.guid) // @todo: potential hack attempt. use disciplinary measure?
        return;

    // verify that indeed the client is replying with the changes that were send to him
    if (!mover->HasPendingMovementChange())
    {
        TC_LOG_INFO("cheat", "WorldSession::HandleMoveKnockBackAck: Player %s from account id %u kicked because no movement change ack was expected from this player",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();
        return;
    }

    PlayerMovementPendingChange pendingChange = mover->PopPendingMovementChange();
    if (pendingChange.movementCounter != movementCounter || pendingChange.movementChangeType != KNOCK_BACK
        || std::fabs(pendingChange.knockbackInfo.speedXY - movementInfo.jump.xyspeed) > 0.01f
        || std::fabs(pendingChange.knockbackInfo.speedZ - movementInfo.jump.zspeed) > 0.01f
        || std::fabs(pendingChange.knockbackInfo.vcos - movementInfo.jump.cosAngle) > 0.01f
        || std::fabs(pendingChange.knockbackInfo.vsin - movementInfo.jump.sinAngle) > 0.01f)
    {
        TC_LOG_INFO("cheat", "WorldSession::HandleMoveKnockBackAck: Player %s from account id %u kicked for incorrect data returned in an ack",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();
        return;
    }

    /* the client data has been verified. let's do the actual change now */
    TC_LOG_ERROR("custom", "POST-VALIDATION received knockback ack. movement counter: %u. vcos: %f, vsin: %f, speedXY: %f, speedZ: %f", movementInfo.guid, movementInfo.jump.cosAngle, movementInfo.jump.sinAngle, movementInfo.jump.xyspeed, movementInfo.jump.zspeed);
    mover->UpdateMovementInfo(movementInfo);
    MovementPacketSender::SendKnockBackToObservers(mover, movementInfo.jump.cosAngle, movementInfo.jump.sinAngle, movementInfo.jump.xyspeed, movementInfo.jump.zspeed);
}

void WorldSession::HandleMoveHoverAck(WorldPacket& recvData)
{
    TC_LOG_DEBUG("network", "CMSG_MOVE_HOVER_ACK");

    ObjectGuid guid;                                        // guid - unused
    recvData >> guid.ReadAsPacked();

    recvData.read_skip<uint32>();                           // unk

    MovementInfo movementInfo;
    movementInfo.FillContentFromPacket(&recvData);

    recvData.read_skip<uint32>();                           // unk2
}

void WorldSession::HandleMoveWaterWalkAck(WorldPacket& recvData)
{
    TC_LOG_DEBUG("network", "CMSG_MOVE_WATER_WALK_ACK");

    ObjectGuid guid;                                        // guid - unused
    recvData >> guid.ReadAsPacked();

    recvData.read_skip<uint32>();                           // unk

    MovementInfo movementInfo;
    movementInfo.FillContentFromPacket(&recvData);

    recvData.read_skip<uint32>();                           // unk2
}

void WorldSession::HandleMoveSetCanFlyAckOpcode(WorldPacket& recvData)
{
    // fly mode on/off
    TC_LOG_DEBUG("network", "WORLD: CMSG_MOVE_SET_CAN_FLY_ACK");

    ObjectGuid guid;                                            // guid - unused
    recvData >> guid.ReadAsPacked();

    recvData.read_skip<uint32>();                          // unk

    MovementInfo movementInfo;
    movementInfo.guid = guid;
    movementInfo.FillContentFromPacket(&recvData);

    recvData.read_skip<float>();                           // unk2

    _player->m_unitMovedByMe->SetUnitMovementFlags(movementInfo.GetMovementFlags());
}

void WorldSession::HandleFeatherFallAck(WorldPacket& recvData)
{
    TC_LOG_DEBUG("network", "WORLD: CMSG_MOVE_FEATHER_FALL_ACK");

    // no used
    recvData.rfinish();                       // prevent warnings spam
}

void WorldSession::HandleMoveUnRootAck(WorldPacket& recvData)
{
    // no used
    recvData.rfinish();                       // prevent warnings spam
                                              /*
                                              uint64 guid;
                                              recvData >> guid;

                                              // now can skip not our packet
                                              if (_player->GetGUID() != guid)
                                              {
                                              recvData.rfinish();                   // prevent warnings spam
                                              return;
                                              }

                                              TC_LOG_DEBUG("network", "WORLD: CMSG_FORCE_MOVE_UNROOT_ACK");

                                              recvData.read_skip<uint32>();                          // unk

                                              MovementInfo movementInfo;
                                              movementInfo.guid = guid;
                                              movementInfo.FillContentFromPacket(&recvData);
                                              recvData.read_skip<float>();                           // unk2
                                              */
}

void WorldSession::HandleMoveRootAck(WorldPacket& recvData)
{
    // no used
    recvData.rfinish();                       // prevent warnings spam
                                              /*
                                              uint64 guid;
                                              recvData >> guid;

                                              // now can skip not our packet
                                              if (_player->GetGUID() != guid)
                                              {
                                              recvData.rfinish();                   // prevent warnings spam
                                              return;
                                              }

                                              TC_LOG_DEBUG("network", "WORLD: CMSG_FORCE_MOVE_ROOT_ACK");

                                              recvData.read_skip<uint32>();                          // unk

                                              MovementInfo movementInfo;
                                              movementInfo.FillContentFromPacket(&recvData);
                                              */
}

void WorldSession::HandleMoveTimeSkippedOpcode(WorldPacket& recvData)
{
    /*  WorldSession::Update(getMSTime());*/
    TC_LOG_DEBUG("network", "WORLD: Received CMSG_MOVE_TIME_SKIPPED");

    ObjectGuid guid;
    recvData >> guid.ReadAsPacked();
    recvData.read_skip<uint32>();
    /*
    uint64 guid;
    uint32 time_skipped;
    recvData >> guid;
    recvData >> time_skipped;
    TC_LOG_DEBUG("network", "WORLD: CMSG_MOVE_TIME_SKIPPED");

    //// @todo
    must be need use in Trinity
    We substract server Lags to move time (AntiLags)
    for exmaple
    GetPlayer()->ModifyLastMoveTime(-int32(time_skipped));
    */
}

void WorldSession::HandleWorldTeleportOpcode(WorldPacket& recvData)
{
    uint32 time;
    uint32 mapid;
    float PositionX;
    float PositionY;
    float PositionZ;
    float Orientation;

    recvData >> time;                                      // time in m.sec.
    recvData >> mapid;
    recvData >> PositionX;
    recvData >> PositionY;
    recvData >> PositionZ;
    recvData >> Orientation;                               // o (3.141593 = 180 degrees)

    TC_LOG_DEBUG("network", "WORLD: Received CMSG_WORLD_TELEPORT");

    if (GetPlayer()->IsInFlight())
    {
        TC_LOG_DEBUG("network", "Player '%s' (GUID: %u) in flight, ignore worldport command.",
            GetPlayer()->GetName().c_str(), GetPlayer()->GetGUID().GetCounter());
        return;
    }

    TC_LOG_DEBUG("network", "CMSG_WORLD_TELEPORT: Player = %s, Time = %u, map = %u, x = %f, y = %f, z = %f, o = %f",
        GetPlayer()->GetName().c_str(), time, mapid, PositionX, PositionY, PositionZ, Orientation);

    if (HasPermission(rbac::RBAC_PERM_OPCODE_WORLD_TELEPORT))
        GetPlayer()->TeleportTo(mapid, PositionX, PositionY, PositionZ, Orientation);
    else
        SendNotification(LANG_YOU_NOT_HAVE_PERMISSION);
}

void WorldSession::HandleSummonResponseOpcode(WorldPacket& recvData)
{
    if (!_player->IsAlive() || _player->IsInCombat())
        return;

    ObjectGuid summoner_guid;
    bool agree;
    recvData >> summoner_guid;
    recvData >> agree;

    _player->SummonIfPossible(agree);
}
