
#include "ShopMgr.h"
#include <iostream>
#include <boost/tokenizer.hpp>

ShopMgr::ShopMgr()
{

}

ShopMgr::~ShopMgr()
{

}

ShopMgr* ShopMgr::instance()
{
    static ShopMgr instance;
    return &instance;
}

bool ShopMgr::HandlePurchaseOrder(std::string order)
{
    boost::char_separator<char> sep("#");
    boost::tokenizer<boost::char_separator<char>> tokens(order, sep);
    int i = 1;
    uint32 orderId;
    uint32 characterId;
    uint32 itemId;
    for (const auto& t : tokens) {
        switch (i)
        {
            case 1:
                orderId = atoul(t.c_str());
                break;
            case 2:
                characterId = atoul(t.c_str());
                break;
            case 3:
                itemId = atoul(t.c_str());
                break;
            default:
                std::cerr << "too many arguments in ShopMgr::HandlePurchaseOrder" << std::endl;
                return false;
        }
        i++;
    }

    if (i != 4)
    {
        std::cerr << "too few arguments in ShopMgr::HandlePurchaseOrder" << std::endl;
        return false;
    }

    std::cout << "Handle order" << std::endl;
    std::cout << "orderId: " << orderId << std::endl;
    std::cout << "characterId: " << characterId << std::endl;
    std::cout << "itemId: " << itemId << std::endl;



    return false;
}
