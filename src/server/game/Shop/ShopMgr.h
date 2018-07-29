
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
        bool HandlePurchaseOrder(std::string order);

};

#define sShopMgr ShopMgr::instance()

#endif
