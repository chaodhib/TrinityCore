
#ifndef _SHOPMGR_H
#define _SHOPMGR_H

#include "Define.h"

class TC_GAME_API ShopMgr
{
    private:
        ShopMgr();
        ~ShopMgr();

    public:
        static ShopMgr* instance();
        bool HandlePurchaseOrder(uint32 &orderId, uint32 &characterId, std::list<std::pair<uint32, uint32> > &itemQuantityList);
};

#define sShopMgr ShopMgr::instance()

#endif
