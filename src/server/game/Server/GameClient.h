/*
 * Copyright (C) 2008-2019 TrinityCore <https://www.trinitycore.org/>
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

#ifndef __GAMECLIENT_H
#define __GAMECLIENT_H

class Player;
class Unit;
class WorldSession;

class TC_GAME_API GameClient
{
    public:
        GameClient(WorldSession* _sessionToServer);
       // void InitBasePlayer(Player* player);

        void InsertIntoClientControlSet(ObjectGuid guid);
        void RemoveFromClientControlSet(ObjectGuid guid);
        bool IsInClientControlSet(ObjectGuid guid) const;

       // void SendDirectMessage(WorldPacket const* data) const;
       // void SetClientControl(Unit* target, bool allowMove);

       // // player unit origianly controlled by this client when it entered the server.
       // Player* GetBasePlayer() const;

       //// unit currently controlled by this client (most of the time, it will be "basePlayer". at times it will be vehicles or other units (when using Mind Control on an enemy for example).
       // Unit* currentlyControlledUnitServerPov;
       // Unit* currentlyControlledUnitClientPov;

        Unit* unitBeingMoved;

    private:
        // describe all units that this client has direct control over. Example, a player on a vehicle has client control over himself and the vehicle at the same time.
        GuidSet allowedClientControl;

        //bool isBasePlayerSet = false;
        WorldSession* sessionToServer;
};

#endif // __GAMECLIENT_H
