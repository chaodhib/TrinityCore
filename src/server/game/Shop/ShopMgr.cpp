
#include "ShopMgr.h"
#include <iostream>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string/classification.hpp> // Include boost::for is_any_of
#include <boost/algorithm/string/split.hpp> // Include for boost::split
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

/*
proper format is ${ITEM_ENTRY_ID}:${QUANTITY}
*/
bool ShopMgr::IsValidItemQuantityEntry(const std::string st)
{
    std::vector<std::string> words;
    boost::split(words, st, boost::is_any_of(":"), boost::token_compress_on);
    if (words.size() != 2)
        return false;

    return atoul(words[0].c_str()) != 0 && atoul(words[1].c_str()) != 0;
}

std::pair<uint32, uint32> ShopMgr::ParseItemQuantityMapEntry(const std::string st)
{
    std::vector<std::string> words;
    boost::split(words, st, boost::is_any_of(":"), boost::token_compress_on);

    return std::pair<uint32, uint32>(atoul(words[0].c_str()), atoul(words[1].c_str()));
}

bool ShopMgr::HandlePurchaseOrder(std::string order)
{
    boost::char_separator<char> sep("#");
    boost::tokenizer<boost::char_separator<char>> tokens(order, sep);
    int i = 1;
    uint32 orderId;
    uint32 characterId;
    std::list<std::pair<uint32, uint32> > itemQuantityList;
    for (const auto& t : tokens) {
        if (i > 102)
        {
            std::cerr << "too many arguments in ShopMgr::HandlePurchaseOrder" << std::endl;
            return false;
        }

        switch (i)
        {
            case 1:
                orderId = atoul(t.c_str());
                break;
            case 2:
                characterId = atoul(t.c_str());
                break;
            default:
                if(IsValidItemQuantityEntry(t))
                    itemQuantityList.push_back(ParseItemQuantityMapEntry(t));
                else
                {
                    std::cerr << "invalid input for items in ShopMgr::HandlePurchaseOrder" << std::endl;
                    return false;
                }
                break;
        }
        i++;
    }

    std::cout << "Handle order" << std::endl;
    std::cout << "orderId: " << orderId << std::endl;
    std::cout << "characterId: " << characterId << std::endl;
    for (const auto& t : itemQuantityList) {
        uint32 itemEntry = t.first;
        uint32 quantity = t.second;

        std::cout << "itemEntry: " << itemEntry << " quantity: " << quantity << std::endl;
    }

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

    // check if character exists @todo: should be in the same transaction as the creation of the item below.
    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHECK_GUID);
    stmt->setUInt32(0, characterId);
    result = CharacterDatabase.Query(stmt);

    if (!result)
    {
        std::cout << "order " << orderId << " aborted. Character " + std::to_string(characterId) + " does not exists!" << std::endl;
        return true;
    }
    
    // check if item template exists
    for (const auto& t : itemQuantityList) {
        uint32 itemEntry = t.first;
        uint32 quantity = t.second;

        stmt = WorldDatabase.GetPreparedStatement(WORLD_CHK_ITEM_TEMPLATE);
        stmt->setUInt32(0, itemEntry);
        result = WorldDatabase.Query(stmt);

        if (!result)
        {
            std::cout << "order " << orderId << " aborted. Item " + std::to_string(itemEntry) + " does not exists!" << std::endl;
            return true;
        }
    }

    // open transaction
    SQLTransaction trans = CharacterDatabase.BeginTransaction();

    // part 1: instantiate the items and send the mail
    MailSender sender(MAIL_CREATURE, 34337 /* The Postmaster */);
    MailDraft draft("Order #" + std::to_string(orderId), "Thank you for your purchase!");

    for (const auto& t : itemQuantityList) {
        uint32 itemEntry = t.first;
        uint32 quantity = t.second;

        if (Item* item = Item::CreateItem(itemEntry, quantity, nullptr))
        {
            item->SaveToDB(trans);
            draft.AddItem(item);
        }
    }

    Player* receiver = ObjectAccessor::FindConnectedPlayer(ObjectGuid(HighGuid::Player, characterId));

    draft.SendMailTo(trans, MailReceiver(receiver, characterId), sender);


    // part 2: add the order to the processed orders table
    stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_PROCESSED_ORDER);
    stmt->setUInt32(0, orderId);
    trans->Append(stmt);

    // commit everything
    CharacterDatabase.CommitTransaction(trans);

    std::cout << "order " << orderId << " delivered succesfuly!" << std::endl;

    return true;
}
