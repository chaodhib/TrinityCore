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
    player->GetMovementInfo().WriteContentIntoPacket(&data);
    player->GetSession()->SendPacket(&data);
}

void MovementPacketSender::SendTeleportPacket(Unit* unit)
{
    WorldPacket data(MSG_MOVE_TELEPORT, 38); // @todo: size here is wrong. a MOVE_TELEPORT can be bigger
    unit->GetMovementInfo().WriteContentIntoPacket(&data, true);
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

void MovementPacketSender::SendSpeedChangeToMover(Unit* unit, UnitMoveType mtype, float newRate)
{
    Player* mover = unit->GetPlayerMovingMe();
    if (!mover)
    {
        TC_LOG_ERROR("TODO", "MovementPacketSender::SendMovementFlagChangeToMover: Incorrect use of the function. It was called on a unit controlled by the server!");
        return;
    }

    WorldPacket data;
    data.Initialize(moveTypeToOpcode[mtype][1], mtype != MOVE_RUN ? 8 + 4 + 4 : 8 + 4 + 1 + 4);
    data << unit->GetPackGUID();
    data << unit->GetMovementCounterAndInc();
    if (mtype == MOVE_RUN)
        data << uint8(1);                               // unknown byte added in 2.1.0
    float newSpeedFlat = newRate * (mover->IsControlledByPlayer() ? playerBaseMoveSpeed[mtype] : baseMoveSpeed[mtype]); // this line is a fucking mess. what if the unit is a creature MCed by a player? this whole speed rate thing needs to die. In the meantime: use mover or unit ?
    data << newSpeedFlat;
    mover->GetSession()->SendPacket(&data);
}

void MovementPacketSender::SendSpeedChangeToObservers(Unit* unit, UnitMoveType mtype)
{
    Player* mover = unit->GetPlayerMovingMe();
    if (!mover)
    {
        TC_LOG_ERROR("TODO", "MovementPacketSender::SendMovementFlagChangeToMover: Incorrect use of the function. It was called on a unit controlled by the server!");
        return;
    }

    WorldPacket data;
    data.Initialize(moveTypeToOpcode[mtype][2], 8 + 30 + 4);
    unit->GetMovementInfo().WriteContentIntoPacket(&data, true);
    data << unit->GetSpeed(mtype);
    unit->SendMessageToSet(&data, mover);
}

void MovementPacketSender::SendSpeedChangeServerMoved(Unit* unit, UnitMoveType mtype)
{
    WorldPacket data;
    data.Initialize(moveTypeToOpcode[mtype][0], 8 + 4);
    data << unit->GetPackGUID();
    data << unit->GetSpeed(mtype);
    unit->SendMessageToSet(&data, true);
}

void MovementPacketSender::SendKnockBackToMover(Player* player, float vcos, float vsin, float speedXY, float speedZ)
{
    WorldPacket data(SMSG_MOVE_KNOCK_BACK, (8 + 4 + 4 + 4 + 4 + 4));
    data << player->GetPackGUID();
    data << player->GetMovementCounterAndInc();
    data << float(vcos);                                    // x direction
    data << float(vsin);                                    // y direction
    data << float(speedXY);                                 // Horizontal speed
    data << float(-speedZ);                                 // Z Movement speed (vertical)

    player->GetSession()->SendPacket(&data);
}

void MovementPacketSender::SendKnockBackToObservers(Player* player)
{
    WorldPacket data(MSG_MOVE_KNOCK_BACK, 66);
    player->GetMovementInfo().WriteContentIntoPacket(&data, true);
    data << player->GetMovementInfo().jump.sinAngle;
    data << player->GetMovementInfo().jump.cosAngle;
    data << player->GetMovementInfo().jump.xyspeed;
    data << player->GetMovementInfo().jump.zspeed;

    player->SendMessageToSet(&data, false);
}

//    // apply
//    {
//        // Step1 Sent by the server to the mover's client       // Step2 Sent back by the mover's client to the server      // Step3 Sent to observers (all of these should be renamed to SMSG! Confirmed by sniff analysis)
//        { SMSG_FORCE_MOVE_ROOT,                                 CMSG_FORCE_MOVE_ROOT_ACK,                                   MSG_MOVE_ROOT },
//        { SMSG_MOVE_WATER_WALK,                                 CMSG_MOVE_WATER_WALK_ACK,                                   MSG_MOVE_WATER_WALK },
//        { SMSG_MOVE_SET_HOVER,                                  CMSG_MOVE_HOVER_ACK,                                        MSG_MOVE_HOVER },
//        { SMSG_MOVE_SET_CAN_FLY,                                CMSG_MOVE_SET_CAN_FLY_ACK,                                  MSG_MOVE_UPDATE_CAN_FLY },
//        { SMSG_MOVE_SET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY,    CMSG_MOVE_SET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY_ACK,      MSG_MOVE_UPDATE_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY },
//        { SMSG_MOVE_FEATHER_FALL,                               CMSG_MOVE_FEATHER_FALL_ACK,                                 MSG_MOVE_FEATHER_FALL },
//        { SMSG_MOVE_GRAVITY_ENABLE,                             CMSG_MOVE_GRAVITY_ENABLE_ACK,                               MSG_MOVE_GRAVITY_CHNG } // todo: confirm these
//    },
//    // unapply
//    {
//        { SMSG_FORCE_MOVE_UNROOT,                               CMSG_FORCE_MOVE_UNROOT_ACK,                                 MSG_MOVE_UNROOT },
//        { SMSG_MOVE_LAND_WALK,                                  CMSG_MOVE_WATER_WALK_ACK,                                   MSG_MOVE_WATER_WALK },
//        { SMSG_MOVE_UNSET_HOVER,                                CMSG_MOVE_HOVER_ACK,                                        MSG_MOVE_HOVER },
//        { SMSG_MOVE_UNSET_CAN_FLY,                              CMSG_MOVE_SET_CAN_FLY_ACK,                                  MSG_MOVE_UPDATE_CAN_FLY },
//        { SMSG_MOVE_UNSET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY,  CMSG_MOVE_SET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY_ACK,      MSG_MOVE_UPDATE_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY },
//        { SMSG_MOVE_NORMAL_FALL,                                CMSG_MOVE_FEATHER_FALL_ACK,                                 MSG_MOVE_FEATHER_FALL },
//        { SMSG_MOVE_GRAVITY_DISABLE,                            CMSG_MOVE_GRAVITY_DISABLE_ACK,                              MSG_MOVE_GRAVITY_CHNG } // todo: confirm these
//    }


void MovementPacketSender::SendMovementFlagChangeToMover(Unit* unit, MovementFlags mFlag, bool apply)
{
    Player* mover = unit->GetPlayerMovingMe();
    if (!mover)
    {
        TC_LOG_ERROR("TODO", "MovementPacketSender::SendMovementFlagChangeToMover: Incorrect use of the function. It was called on a unit controlled by the server!");
        return;
    }

    Opcodes opcode;
    switch (mFlag)
    {
        case MOVEMENTFLAG_ROOT:                 opcode = apply ? SMSG_FORCE_MOVE_ROOT :         SMSG_FORCE_MOVE_UNROOT; break;
        case MOVEMENTFLAG_WATERWALKING:         opcode = apply ? SMSG_MOVE_WATER_WALK :         SMSG_MOVE_LAND_WALK; break;
        case MOVEMENTFLAG_HOVER:                opcode = apply ? SMSG_MOVE_SET_HOVER :          SMSG_MOVE_UNSET_HOVER; break;
        case MOVEMENTFLAG_CAN_FLY:              opcode = apply ? SMSG_MOVE_SET_CAN_FLY :        SMSG_MOVE_UNSET_CAN_FLY; break;
        case MOVEMENTFLAG_FALLING_SLOW:         opcode = apply ? SMSG_MOVE_FEATHER_FALL :       SMSG_MOVE_NORMAL_FALL; break;
        case MOVEMENTFLAG_DISABLE_GRAVITY:      opcode = apply ? SMSG_MOVE_GRAVITY_DISABLE :    SMSG_MOVE_GRAVITY_ENABLE; break;
        default:
            TC_LOG_ERROR("TODO", "MovementPacketSender::SendMovementFlagChangeServerMoved: Unsupported MovementFlag (%d), data not sent to client.", mFlag);
            return;
    }

    WorldPacket data(opcode, 12 + 4); // is the upper bound of unit->GetPackGUID().size() indeed equal to 12?
    data << unit->GetPackGUID();
    data << unit->GetMovementCounterAndInc();
    mover->GetSession()->SendPacket(&data);
}

void MovementPacketSender::SendMovementFlagChangeToMover(Unit* unit, MovementFlags2 mFlag, bool apply)
{
    Player* mover = unit->GetPlayerMovingMe();
    if (!mover)
    {
        TC_LOG_ERROR("TODO", "MovementPacketSender::SendMovementFlagChangeToMover: Incorrect use of the function. It was called on a unit controlled by the server!");
        return;
    }

    Opcodes opcode;
    switch (mFlag)
    {
        case MOVEMENTFLAG2_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY: opcode = apply ? SMSG_MOVE_SET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY : SMSG_MOVE_UNSET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY; break;
        default:
            TC_LOG_ERROR("TODO", "MovementPacketSender::SendMovementFlagChangeServerMoved: Unsupported MovementFlag2 (%d), data not sent to client.", mFlag);
            return;
    }

    WorldPacket data(opcode, 12 + 4); // is the upper bound of unit->GetPackGUID().size() indeed equal to 12?
    data << unit->GetPackGUID();
    data << unit->GetMovementCounterAndInc();
    mover->GetSession()->SendPacket(&data);
}

void MovementPacketSender::SendMovementFlagChangeToObservers(Unit* unit, MovementFlags mFlag)
{
    Opcodes opcode;
    switch (mFlag)
    {
        case MOVEMENTFLAG_ROOT:                 opcode = MSG_MOVE_ROOT; break;
        case MOVEMENTFLAG_WATERWALKING:         opcode = MSG_MOVE_WATER_WALK; break;
        case MOVEMENTFLAG_HOVER:                opcode = MSG_MOVE_HOVER; break;
        case MOVEMENTFLAG_CAN_FLY:              opcode = MSG_MOVE_UPDATE_CAN_FLY; break;
        case MOVEMENTFLAG_FALLING_SLOW:         opcode = MSG_MOVE_FEATHER_FALL; break;
        case MOVEMENTFLAG_DISABLE_GRAVITY:      opcode = MSG_MOVE_GRAVITY_CHNG; break;
        default:
            TC_LOG_ERROR("TODO", "MovementPacketSender::SendMovementFlagChangeServerMoved: Unsupported MovementFlag (%d), data not sent to client.", mFlag);
            return;
    }

    WorldPacket data(opcode, 64);
    unit->GetMovementInfo().WriteContentIntoPacket(&data, true);
    unit->SendMessageToSet(&data, false);
}

void MovementPacketSender::SendMovementFlagChangeToObservers(Unit* unit, MovementFlags2 mFlag)
{
    Opcodes opcode;
    switch (mFlag)
    {
        case MOVEMENTFLAG2_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY:     opcode = MSG_MOVE_UPDATE_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY; break;
        default:
            TC_LOG_ERROR("TODO", "MovementPacketSender::SendMovementFlagChangeServerMoved: Unsupported MovementFlag (%d), data not sent to client.", mFlag);
            return;
    }

    WorldPacket data(opcode, 64);
    unit->GetMovementInfo().WriteContentIntoPacket(&data, true);
    unit->SendMessageToSet(&data, false);
}

void MovementPacketSender::SendMovementFlagChangeServerMoved(Unit* unit, MovementFlags mFlag, bool apply)
{
    // MOVEMENTFLAG_CAN_FLY & MOVEMENTFLAG2_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY have no equivalent to the player controlled opcodes.
    Opcodes opcode;
    switch (mFlag)
    {
        case MOVEMENTFLAG_DISABLE_GRAVITY:  opcode = apply ? SMSG_SPLINE_MOVE_GRAVITY_DISABLE   : SMSG_SPLINE_MOVE_GRAVITY_ENABLE; break;
        case MOVEMENTFLAG_ROOT:             opcode = apply ? SMSG_SPLINE_MOVE_ROOT              : SMSG_SPLINE_MOVE_UNROOT; break;
        case MOVEMENTFLAG_WATERWALKING:     opcode = apply ? SMSG_SPLINE_MOVE_WATER_WALK        : SMSG_SPLINE_MOVE_LAND_WALK; break;
        case MOVEMENTFLAG_FALLING_SLOW:     opcode = apply ? SMSG_SPLINE_MOVE_FEATHER_FALL      : SMSG_SPLINE_MOVE_NORMAL_FALL; break;
        case MOVEMENTFLAG_HOVER:            opcode = apply ? SMSG_SPLINE_MOVE_SET_HOVER         : SMSG_SPLINE_MOVE_UNSET_HOVER; break;
        default:
            TC_LOG_ERROR("TODO", "MovementPacketSender::SendMovementFlagChangeServerMoved: Unsupported MovementFlag (%d), data not sent to client.", mFlag);
            return;
    }

    WorldPacket data;
    data.Initialize(opcode, unit->GetPackGUID().size() + 4);
    data << unit->GetPackGUID();
    unit->SendMessageToSet(&data, true);
}

 void MovementPacketSender::SendMovementFlagChange(Unit* unit, MovementFlags mFlag, bool apply)
{
    if (unit->IsMovedByPlayer())
        SendMovementFlagChangeToMover(unit, mFlag, apply);
    else
        SendMovementFlagChangeServerMoved(unit, mFlag, apply);
}

 void MovementPacketSender::SendMovementFlagChange(Unit* unit, MovementFlags2 mFlag, bool apply)
 {
     if (unit->IsMovedByPlayer())
         SendMovementFlagChangeToMover(unit, mFlag, apply);
     else
         TC_LOG_ERROR("TODO", "MovementPacketSender::SendMovementFlagChange: Unsupported MovementFlag2 (%d), data not sent to client.", mFlag);
 }