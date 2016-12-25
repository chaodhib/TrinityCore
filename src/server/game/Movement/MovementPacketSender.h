#ifndef MOVEMENT_SENDER_H
#define MOVEMENT_SENDER_H

#include "Unit.h"

class Player;

class MovementPacketSender
{

public:
    static void SendHeightChange(Player* player, uint32 movementCounter, bool mounted);
    static void SendTeleportAckPacket(Player* player, uint32 movementCounter);
    static void SendTeleportPacket(Unit* unit);

    /*
    Only use this method if the moving unit is controlled/moved by a player
    */
    static void SendSpeedChangeToMover(Unit* movingUnit, Player* mover, UnitMoveType mtype, uint32 movementCounter);

    /*
    Only use this method if the moving unit is controlled/moved by a player
    */
    static void SendSpeedChangeToObservers(Unit* movingUnit, Player* mover, UnitMoveType mtype, uint32 movementCounter);

    /*
    Only use this method if the moving unit is controlled/moved by the server
    */
    static void SendSpeedChange(Unit* movingUnit, UnitMoveType mtype);
private:
    // Spline packets are for units controlled by AI. "Force speed change" (wrongly named opcodes) and "move set speed" packets are for units controlled by a player.
    static Opcodes const moveTypeToOpcode[MAX_MOVE_TYPE][3]; 
};

#endif