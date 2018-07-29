
#include "ShopMgr.h"
#include <iostream>
#include <boost/tokenizer.hpp>
#include "Mail.h"
#include "Item.h"

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
    uint32 itemEntry;
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
                itemEntry = atoul(t.c_str());
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
    std::cout << "itemEntry: " << itemEntry << std::endl;

    // check if the order was already processed
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_PROCESSED_ORDER);
    stmt->setUInt32(0, orderId);
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (result)
    {
        std::cout << "order " << orderId << " already processed. skipping." << std::endl;
        return true;
    }

    std::cout << "new order " << orderId << "." << std::endl;

    // check if character exists
    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHECK_GUID);
    stmt->setUInt32(0, characterId);
    result = CharacterDatabase.Query(stmt);

    if (!result)
    {
        std::cout << "order " << orderId << " aborted. Character " + std::to_string(characterId) + " does not exists!" << std::endl;
        return true;
    }
    
    // check if item template exists
    stmt = WorldDatabase.GetPreparedStatement(WORLD_CHK_ITEM_TEMPLATE);
    stmt->setUInt32(0, itemEntry);
    result = WorldDatabase.Query(stmt);

    if (!result)
    {
        std::cout << "order " << orderId << " aborted. Item " + std::to_string(itemEntry) + " does not exists!" << std::endl;
        return true;
    }

    // open transaction
    SQLTransaction trans = CharacterDatabase.BeginTransaction();

    // part 1: instantiate the item and send the mail
    MailSender sender(MAIL_CREATURE, 34337 /* The Postmaster */);
    MailDraft draft("Order #" + std::to_string(orderId), "Thank you for your purchase!");

    if (Item* item = Item::CreateItem(itemEntry, 1, nullptr))
    {
        item->SaveToDB(trans);
        draft.AddItem(item);
    }

    draft.SendMailTo(trans, MailReceiver(characterId), sender);

    // part 2: add the order to the processed orders table
    stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_PROCESSED_ORDER);
    stmt->setUInt32(0, orderId);
    trans->Append(stmt);

    // commit everything
    CharacterDatabase.CommitTransaction(trans);

    std::cout << "order " << orderId << " delivered succesfuly!" << std::endl;

    return true;
}
