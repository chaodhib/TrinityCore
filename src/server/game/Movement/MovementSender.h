#ifndef MOVEMENT_SENDER_H
#define MOVEMENT_SENDER_H

class Player;

class MovementSender
{

public:
    static void SendHeightChange(Player* player, uint32 movementCounter, bool mounted);
    static void SendTeleportAckPacket(Player* player, uint32 movementCounter);
};

#endif