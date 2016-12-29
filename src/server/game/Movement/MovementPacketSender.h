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
#include "Player.h"

class Player;

class MovementPacketSender
{
    public:
        static void SendHeightChange(Player* player, bool mounted);
        static void SendTeleportAckPacket(Player* player);
        static void SendTeleportPacket(Unit* unit);

        // Only use this method if the moving unit is controlled/moved by a player
        static void SendSpeedChangeToMover(Unit* unit, UnitMoveType mtype, float newRate);
        static void SendSpeedChangeToObservers(Unit* unit, UnitMoveType mtype);
        // Only use this method if the moving unit is controlled/moved by the server
        static void SendSpeedChangeServerMoved(Unit* unit, UnitMoveType mtype);

        static void SendKnockBackToMover(Player* player, float vcos, float vsin, float speedXY, float speedZ);
        static void SendKnockBackToObservers(Player* player);

    public:
        static void SendMovementFlagChange(Unit* unit, MovementFlags mFlag, bool apply);
        static void SendMovementFlagChange(Unit* unit, MovementFlags2 mFlag, bool apply);

    private:
        // the follow 4 methods are to be used on a unit controlled & moved by a player (as in direct client control: possess, vehicule,..)
        static void SendMovementFlagChangeToMover(Unit* unit, MovementFlags mFlag, bool apply);
        static void SendMovementFlagChangeToMover(Unit* unit, MovementFlags2 mFlag, bool apply);
        static void SendMovementFlagChangeToObservers(Unit* unit, MovementFlags mFlag);
        static void SendMovementFlagChangeToObservers(Unit* unit, MovementFlags2 mFlag);

        // to be used on a unit controlled & moved by the server
        static void SendMovementFlagChangeServerMoved(Unit* unit, MovementFlags mFlag, bool apply);

    private:
        static Opcodes const moveTypeToOpcode[MAX_MOVE_TYPE][3];
};

#endif