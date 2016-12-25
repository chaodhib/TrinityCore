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

#include "MovementSender.h"
#include "Player.h"
#include "WorldPacket.h"
#include "WorldSession.h"

void MovementSender::SendHeightChange(Player* player, uint32 movementCounter, bool mounted)
{
    WorldPacket data(SMSG_MOVE_SET_COLLISION_HGT, player->GetPackGUID().size() + 4 + 4);
    data << player->GetPackGUID();
    data << movementCounter;
    data << player->GetCollisionHeight(mounted);
    player->GetSession()->SendPacket(&data);
}

void MovementSender::SendTeleportAckPacket(Player* player, uint32 movementCounter)
{
    WorldPacket data(MSG_MOVE_TELEPORT_ACK, 41);
    data << player->GetPackGUID();
    data << uint32(movementCounter);
    player->BuildMovementPacket(&data);
    player->GetSession()->SendPacket(&data);
}

void MovementSender::SendTeleportPacket(Unit* unit)
{
    WorldPacket data(MSG_MOVE_TELEPORT, 38);
    data << unit->GetPackGUID();
    unit->BuildMovementPacket(&data);
    unit->SendMessageToSet(&data, false);
}
