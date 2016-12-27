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

#ifndef MOVEMENT_SENDER_H
#define MOVEMENT_SENDER_H

#include "Unit.h"

class Player;

class MovementPacketSender
{

public:
    static void SendHeightChange(Player* player, bool mounted);
    static void SendTeleportAckPacket(Player* player);
    static void SendTeleportPacket(Unit* unit);

    // Only use this method if the moving unit is controlled/moved by a player
    static void SendSpeedChangeToMover(Unit* movingUnit, Player* mover, UnitMoveType mtype);

    // Only use this method if the moving unit is controlled/moved by a player
    static void SendSpeedChangeToObservers(Unit* movingUnit, Player* mover, UnitMoveType mtype);

    // Only use this method if the moving unit is controlled/moved by the server
    static void SendSpeedChange(Unit* movingUnit, UnitMoveType mtype);

    static void SendMovementFlagChange(Unit* unit, PlayerMovementType pType);

    static void SendKnockBackToMover(Player* player, float vcos, float vsin, float speedXY, float speedZ);

    static void SendKnockBackToObservers(Player* player);

    static void SendHoverToMover(Player* player, bool apply);

    static void SendHoverToObservers(Player* player);

private:
    static Opcodes const moveTypeToOpcode[MAX_MOVE_TYPE][3]; 
};

#endif