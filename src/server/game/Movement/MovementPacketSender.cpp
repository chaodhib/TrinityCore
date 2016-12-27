/*
* Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
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

#include "MovementPacketSender.h"
#include "Player.h"
#include "WorldPacket.h"
#include "WorldSession.h"

void MovementPacketSender::SendHeightChange(Player* player, bool mounted)
{
    WorldPacket data(SMSG_MOVE_SET_COLLISION_HGT, player->GetPackGUID().size() + 4 + 4);
    data << player->GetPackGUID();
    data << player->GetMovementCounterAndInc();
    data << player->GetCollisionHeight(mounted);
    player->GetSession()->SendPacket(&data);
}

void MovementPacketSender::SendTeleportAckPacket(Player* player)
{
    WorldPacket data(MSG_MOVE_TELEPORT_ACK, 41);
    data << player->GetPackGUID();
    data << player->GetMovementCounterAndInc();
    player->BuildMovementPacket(&data);
    player->GetSession()->SendPacket(&data);
}

void MovementPacketSender::SendTeleportPacket(Unit* unit)
{
    WorldPacket data(MSG_MOVE_TELEPORT, 38);
    data << unit->GetPackGUID();
    unit->BuildMovementPacket(&data);
    unit->SendMessageToSet(&data, false);
}

// Spline packets are for units controlled by the server. "Force speed change" (wrongly named opcodes) and "move set speed" packets are for units controlled by a player.
Opcodes const MovementPacketSender::moveTypeToOpcode[MAX_MOVE_TYPE][3] =
{
    { SMSG_SPLINE_SET_WALK_SPEED,        SMSG_FORCE_WALK_SPEED_CHANGE,           MSG_MOVE_SET_WALK_SPEED },
    { SMSG_SPLINE_SET_RUN_SPEED,         SMSG_FORCE_RUN_SPEED_CHANGE,            MSG_MOVE_SET_RUN_SPEED },
    { SMSG_SPLINE_SET_RUN_BACK_SPEED,    SMSG_FORCE_RUN_BACK_SPEED_CHANGE,       MSG_MOVE_SET_RUN_BACK_SPEED },
    { SMSG_SPLINE_SET_SWIM_SPEED,        SMSG_FORCE_SWIM_SPEED_CHANGE,           MSG_MOVE_SET_SWIM_SPEED },
    { SMSG_SPLINE_SET_SWIM_BACK_SPEED,   SMSG_FORCE_SWIM_BACK_SPEED_CHANGE,      MSG_MOVE_SET_SWIM_BACK_SPEED },
    { SMSG_SPLINE_SET_TURN_RATE,         SMSG_FORCE_TURN_RATE_CHANGE,            MSG_MOVE_SET_TURN_RATE },
    { SMSG_SPLINE_SET_FLIGHT_SPEED,      SMSG_FORCE_FLIGHT_SPEED_CHANGE,         MSG_MOVE_SET_FLIGHT_SPEED },
    { SMSG_SPLINE_SET_FLIGHT_BACK_SPEED, SMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE,    MSG_MOVE_SET_FLIGHT_BACK_SPEED },
    { SMSG_SPLINE_SET_PITCH_RATE,        SMSG_FORCE_PITCH_RATE_CHANGE,           MSG_MOVE_SET_PITCH_RATE },
};

void MovementPacketSender::SendSpeedChangeToMover(Unit* movingUnit, Player* mover, UnitMoveType mtype)
{
    WorldPacket data;
    data.Initialize(moveTypeToOpcode[mtype][1], mtype != MOVE_RUN ? 8 + 4 + 4 : 8 + 4 + 1 + 4);
    data << movingUnit->GetPackGUID();
    data << mover->GetMovementCounterAndInc();
    if (mtype == MOVE_RUN)
        data << uint8(1);                               // unknown byte added in 2.1.0
    data << movingUnit->GetSpeed(mtype);
    mover->GetSession()->SendPacket(&data);
}

void MovementPacketSender::SendSpeedChangeToObservers(Unit* movingUnit, Player* mover, UnitMoveType mtype)
{
    WorldPacket data;
    data.Initialize(moveTypeToOpcode[mtype][2], 8 + 30 + 4);
    data << movingUnit->GetPackGUID();
    movingUnit->BuildMovementPacket(&data);
    data << movingUnit->GetSpeed(mtype);
    mover->SendMessageToSet(&data, false);
}

void MovementPacketSender::SendSpeedChange(Unit* movingUnit, UnitMoveType mtype)
{
    WorldPacket data;
    data.Initialize(moveTypeToOpcode[mtype][0], 8 + 4);
    data << movingUnit->GetPackGUID();
    data << movingUnit->GetSpeed(mtype);
    movingUnit->SendMessageToSet(&data, false);
}

void MovementPacketSender::SendMovementFlagChange(Unit* unit, PlayerMovementType pType)
{
    if (unit->IsMovedByPlayer())
    {
        Player* player = unit->GetPlayerMovingMe();
        WorldPacket data;
        switch (pType)
        {
        case MOVE_ROOT:       data.Initialize(SMSG_FORCE_MOVE_ROOT, player->GetPackGUID().size() + 4); break;
        case MOVE_UNROOT:     data.Initialize(SMSG_FORCE_MOVE_UNROOT, player->GetPackGUID().size() + 4); break;
        case MOVE_WATER_WALK: data.Initialize(SMSG_MOVE_WATER_WALK, player->GetPackGUID().size() + 4); break;
        case MOVE_LAND_WALK:  data.Initialize(SMSG_MOVE_LAND_WALK, player->GetPackGUID().size() + 4); break;
        default:
            TC_LOG_ERROR("TODO", "MovementPacketSender::SendMovementFlagChange: Unsupported move type (%d), data not sent to client.", pType);
            return;
        }
        data << player->GetPackGUID();
        data << player->GetMovementCounterAndInc();
        player->GetSession()->SendPacket(&data);
    }
    else
    {
        WorldPacket data;
        switch (pType)
        {
        case MOVE_ROOT:       data.Initialize(SMSG_SPLINE_MOVE_ROOT, unit->GetPackGUID().size() + 4); break;
        case MOVE_UNROOT:     data.Initialize(SMSG_SPLINE_MOVE_UNROOT, unit->GetPackGUID().size() + 4); break;
        case MOVE_WATER_WALK: data.Initialize(SMSG_SPLINE_MOVE_WATER_WALK, unit->GetPackGUID().size() + 4); break;
        case MOVE_LAND_WALK:  data.Initialize(SMSG_SPLINE_MOVE_LAND_WALK, unit->GetPackGUID().size() + 4); break;
        default:
            TC_LOG_ERROR("TODO", "MovementPacketSender::SendMovementFlagChange: Unsupported move type (%d), data not sent to client.", pType);
            return;
        }
        data << unit->GetPackGUID();
        unit->SendMessageToSet(&data, false);
    }

}
