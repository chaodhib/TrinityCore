
#include "MessagingMgr.h"
#include "ShopMgr.h"
#include <chrono>
#include <iostream>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string/classification.hpp> // Include boost::for is_any_of
#include <boost/algorithm/string/split.hpp> // Include for boost::split

static int verbosity = 3;
const std::string MessagingMgr::CHARACTER_TOPIC = "CHARACTER";
const std::string MessagingMgr::ACCOUNT_TOPIC = "ACCOUNT";
const std::string MessagingMgr::GEAR_SNAPSHOT_TOPIC = "GEAR_SNAPSHOT";
const std::string MessagingMgr::GEAR_PURCHASE_TOPIC = "GEAR_PURCHASE";
const std::string MessagingMgr::GEAR_PURCHASE_ACK_TOPIC = "GEAR_PURCHASE_ACK";

void EventCb::event_cb(RdKafka::Event &event) {
    switch (event.type())
    {
    case RdKafka::Event::EVENT_ERROR:
        std::cerr << "ERROR (" << RdKafka::err2str(event.err()) << "): " << event.str() << std::endl;
        break;

    case RdKafka::Event::EVENT_STATS:
        std::cerr << "\"STATS\": " << event.str() << std::endl;
        break;

    case RdKafka::Event::EVENT_LOG:
        fprintf(stderr, "LOG-%i-%s: %s\n", event.severity(), event.fac().c_str(), event.str().c_str());
        break;

    default:
        std::cerr << "EVENT " << event.type() << " (" << RdKafka::err2str(event.err()) << "): " <<
            event.str() << std::endl;
        break;
    }
}

void DeliveryReportCb::dr_cb(RdKafka::Message &message) {
    std::cout << "Message delivery for (" << message.len() << " bytes): " << message.errstr() << std::endl;
    std::cout << "Message error code: " << message.err() << std::endl;
    std::cout << message.topic_name() << std::endl;

    if (message.err() == RdKafka::ErrorCode::ERR_NO_ERROR) {

        std::string payload = std::string(static_cast<const char *>(message.payload()));
        std::cout << "Payload: " + payload << std::endl;

        if (message.topic_name() == MessagingMgr::ACCOUNT_TOPIC) {
            uint32 accountId = GetFirstParam(payload);
            PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_ACCOUNT_KAFKA_OK);
            stmt->setUInt32(0, accountId);
            LoginDatabase.DirectExecute(stmt);

        } else if (message.topic_name() == MessagingMgr::CHARACTER_TOPIC) {
            uint32 characterId = GetSecondParam(payload);
            PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_KAFKA_OK);
            stmt->setUInt32(0, characterId);
            CharacterDatabase.DirectExecute(stmt);

        } else if (message.topic_name() == MessagingMgr::GEAR_SNAPSHOT_TOPIC) {
            uint32 characterId = GetFirstParam(payload);
            PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_GEAR_KAFKA_OK);
            stmt->setUInt32(0, characterId);
            CharacterDatabase.DirectExecute(stmt);

        } else if (message.topic_name() == MessagingMgr::GEAR_PURCHASE_ACK_TOPIC) {
            uint32 orderId = GetFirstParam(payload);
            PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_PROCESSED_SHOP_ORDERS_KAFKA_OK);
            stmt->setUInt32(0, orderId);
            CharacterDatabase.DirectExecute(stmt);
        }
    }
}

uint32 DeliveryReportCb::GetFirstParam(std::string payload)
{
    std::vector<std::string> words;
    boost::split(words, payload, boost::is_any_of("#"), boost::token_compress_on);

    return atoul(words[0].c_str());
}

uint32 DeliveryReportCb::GetSecondParam(std::string payload)
{
    std::vector<std::string> words;
    boost::split(words, payload, boost::is_any_of("#"), boost::token_compress_on);

    return atoul(words[1].c_str());
}

void OffsetCommitCb::offset_commit_cb(RdKafka::ErrorCode err, std::vector<RdKafka::TopicPartition*>& offsets)
{
    std::cerr << "Propagate offset for " << offsets.size() << " partitions, error: " << RdKafka::err2str(err) << std::endl;

    /* No offsets to commit, dont report anything. */
    if (err == RdKafka::ERR__NO_OFFSET)
        return;

    /* Send up-to-date records_consumed report to make sure consumed > committed */

    std::cout << "{ " <<
        "\"name\": \"offsets_committed\", " <<
        "\"success\": " << (err ? "false" : "true") << ", " <<
        "\"error\": \"" << (err ? RdKafka::err2str(err) : "") << "\", " <<
        "\"offsets\": [ ";
    assert(offsets.size() > 0);
    for (unsigned int i = 0; i < offsets.size(); i++) {
        std::cout << (i == 0 ? "" : ", ") << "{ " <<
            " \"topic\": \"" << offsets[i]->topic() << "\", " <<
            " \"partition\": " << offsets[i]->partition() << ", " <<
            " \"offset\": " << (int)offsets[i]->offset() << ", " <<
            " \"error\": \"" <<
            (offsets[i]->err() ? RdKafka::err2str(offsets[i]->err()) : "") <<
            "\" " <<
            " }";
    }
    std::cout << " ] }" << std::endl;

}

/*
proper format is ${ITEM_ENTRY_ID}:${QUANTITY}
*/
bool MessagingMgr::IsValidItemQuantityEntry(const std::string st)
{
    std::vector<std::string> words;
    boost::split(words, st, boost::is_any_of(":"), boost::token_compress_on);
    if (words.size() != 2)
        return false;

    return atoul(words[0].c_str()) != 0 && atoul(words[1].c_str()) != 0;
}

std::pair<uint32, uint32> MessagingMgr::ParseItemQuantityMapEntry(const std::string st)
{
    std::vector<std::string> words;
    boost::split(words, st, boost::is_any_of(":"), boost::token_compress_on);

    return std::pair<uint32, uint32>(atoul(words[0].c_str()), atoul(words[1].c_str()));
}

bool MessagingMgr::ParseGearPurchaseMessage(std::string message, uint32 &orderId, uint32 &characterId, std::list<std::pair<uint32, uint32>> &itemQuantityList)
{
    std::vector<std::string> tokens;
    boost::split(tokens, message, boost::is_any_of("#"), boost::token_compress_on);

    if (tokens.size() < 3 || tokens.size() > 102) // allow for maximum 100 stacks of items.
    {
        std::cerr << "too few or too many arguments in MessagingMgr::ParseGearPurchaseMessage" << std::endl;
        return false;
    }

    orderId = atoul(tokens[0].c_str());
    if (orderId == 0)
    {
        std::cerr << "invalid orderId in MessagingMgr::ParseGearPurchaseMessage" << std::endl;
        return false;
    }

    characterId = atoul(tokens[1].c_str());
    if (characterId == 0)
    {
        std::cerr << "invalid characterId in MessagingMgr::ParseGearPurchaseMessage" << std::endl;
        return false;
    }

    for (int i = 2; i < tokens.size(); i++)
    {
        if (IsValidItemQuantityEntry(tokens[i]))
            itemQuantityList.push_back(ParseItemQuantityMapEntry(tokens[i]));
        else
        {
            std::cerr << "invalid input for items in MessagingMgr::ParseGearPurchaseMessage" << std::endl;
            return false;
        }
    }

    return true;
}

void MessagingMgr::HandleGearPurchaseMessage(RdKafka::Message &msg)
{
    uint32 orderId;
    uint32 characterId;
    std::list<std::pair<uint32, uint32> > itemQuantityList;
    bool success;
    bool parseSuccess;

    switch (msg.err()) {
        case RdKafka::ERR__TIMED_OUT:
            break;

        case RdKafka::ERR_NO_ERROR:
            /* Real message */
            if (verbosity >= 3)
                std::cerr << "Read msg at offset " << msg.offset() << std::endl;
            RdKafka::MessageTimestamp ts;
            ts = msg.timestamp();
            if (verbosity >= 2 &&
                ts.type != RdKafka::MessageTimestamp::MSG_TIMESTAMP_NOT_AVAILABLE) {
                std::string tsname = "?";
                if (ts.type == RdKafka::MessageTimestamp::MSG_TIMESTAMP_CREATE_TIME)
                    tsname = "create time";
                else if (ts.type == RdKafka::MessageTimestamp::MSG_TIMESTAMP_LOG_APPEND_TIME)
                    tsname = "log append time";
                std::cout << "Timestamp: " << tsname << " " << ts.timestamp << std::endl;
            }
            if (verbosity >= 2 && msg.key()) {
                std::cout << "Key: " << *msg.key() << std::endl;
            }
            if (verbosity >= 1) {
                printf("%.*s\n",
                    static_cast<int>(msg.len()),
                    static_cast<const char *>(msg.payload()));
            }

            parseSuccess = ParseGearPurchaseMessage(std::string(static_cast<const char *>(msg.payload())), orderId, characterId, itemQuantityList);
            if (!parseSuccess)
            {
                std::cout << "ignoring gear purchase message. invalid format." << std::endl;
                break;
            }

            success = sShopMgr->HandlePurchaseOrder(orderId, characterId, itemQuantityList);
            SendGearPurchaseAck(orderId, success);

            break;

        case RdKafka::ERR__PARTITION_EOF:
            break;

        case RdKafka::ERR__UNKNOWN_TOPIC:
        case RdKafka::ERR__UNKNOWN_PARTITION:
            std::cerr << "Consume failed: " << msg.errstr() << std::endl;
            break;

        default:
            /* Errors */
            std::cerr << "Consume failed: " << msg.errstr() << std::endl;
    }
}

void MessagingMgr::SyncAccounts()
{
    PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_KAFKA_PENDING); // todo: add pagination to the query. to limit size of the ResultSet
    PreparedQueryResult result = LoginDatabase.Query(stmt);

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 accountId = fields[0].GetUInt32();
            std::string username = fields[1].GetString();
            std::string hashedPassword = fields[2].GetString();

            SendAccountSnapshot(accountId, username, hashedPassword);

        } while (result->NextRow());
    }
}

void MessagingMgr::SyncCharactes()
{
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_KAFKA_PENDING); // todo: add pagination to the query. to limit size of the ResultSet
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 characterId = fields[0].GetUInt32();
            uint32 accountId = fields[1].GetUInt32();
            std::string characterName = fields[2].GetString();
            uint8 characterClass = fields[3].GetInt8();

            SendCharacter(accountId, characterId, characterName, characterClass, true);

        } while (result->NextRow());
    }
}

void MessagingMgr::SyncGearSnapshots()
{
    //PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_GEAR_KAFKA_PENDING); // todo: add pagination to the query. to limit size of the ResultSet
    //PreparedQueryResult result = CharacterDatabase.Query(stmt);

    //if (result)
    //{
    //    do
    //    {
    //        Field* fields = result->Fetch();
    //        uint32 characterId = fields[0].GetUInt32();
    //        uint32 accountId = fields[1].GetUInt32();
    //        std::string characterName = fields[2].GetString();
    //        uint8 characterClass = fields[3].GetInt8();

    //        SendCharacter(characterId, accountId, characterName, characterClass);

    //    } while (result->NextRow());
    //}
}

void MessagingMgr::SyncGearPurchaseAcks()
{
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_PROCESSED_SHOP_ORDERS_KAFKA_PENDING); // todo: add pagination to the query. to limit size of the ResultSet
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 orderId = fields[0].GetUInt32();

            SendGearPurchaseAck(orderId, true);

        } while (result->NextRow());
    }
}

MessagingMgr::MessagingMgr()
{
    InitProducer();
    InitConsumer();
    InitGearSnapshotTopic();
    InitAccountTopic();
    InitCharacterTopic();
    InitGearPurchaseAckTopic();
    ConsumerSubscribe();
}

MessagingMgr::~MessagingMgr()
{
    consumer->close();

    delete accountTopic;
    delete gearSnapshotTopic;
    delete characterTopic;
    delete producer;
    delete consumer;
}

void MessagingMgr::InitProducer()
{
    std::string brokers = "localhost";
    std::string errstr;

    RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

    conf->set("metadata.broker.list", brokers, errstr);
    conf->set("event_cb", &event_cb, errstr);
    conf->set("dr_cb", &dr_cb, errstr);

    if (conf->set("acks", "all", errstr) !=
        RdKafka::Conf::CONF_OK) {
        std::cerr << errstr << std::endl;
        exit(1);
    }

    /*
    * Create producer using accumulated global configuration.
    */
    this->producer = RdKafka::Producer::create(conf, errstr);
    if (!producer) {
        std::cerr << "Failed to create producer: " << errstr << std::endl;
        exit(1);
    }

    delete conf;

    std::cout << "% Created producer " << this->producer->name() << std::endl;
}

void MessagingMgr::InitConsumer()
{
    std::string brokers = "localhost";
    std::string errstr;

    RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

    conf->set("metadata.broker.list", brokers, errstr);
    conf->set("offset_commit_cb", &commit_cb, errstr);
    conf->set("event_cb", &event_cb, errstr);
    conf->set("auto.offset.reset", "earliest", errstr);

    if (conf->set("group.id", "1", errstr) != RdKafka::Conf::CONF_OK) {
        std::cerr << errstr << std::endl;
        exit(1);
    }

    /*
    * Create consumer using accumulated global configuration.
    */
    this->consumer = RdKafka::KafkaConsumer::create(conf, errstr);
    if (!this->consumer) {
        std::cerr << "Failed to create consumer: " << errstr << std::endl;
        exit(1);
    }

    delete conf;

    std::cout << "% Created consumer " << this->consumer->name() << std::endl;
}

void MessagingMgr::ConsumerSubscribe()
{
    int64_t start_offset = RdKafka::Topic::OFFSET_STORED;

    std::vector<std::string> topics;
    topics.push_back(std::string("GEAR_PURCHASE"));

    /*
    * Subscribe to topics
    */
    RdKafka::ErrorCode err = consumer->subscribe(topics);
    if (err) {
        std::cerr << "Failed to subscribe to " << topics.size() << " topics: "
            << RdKafka::err2str(err) << std::endl;
        exit(1);
    }

    std::cout << "Consumer is now subscribed to topics" << std::endl;
}

void MessagingMgr::InitGearSnapshotTopic()
{
    std::string errstr;

    /*
    * Create topic handle.
    */
    this->gearSnapshotTopic = RdKafka::Topic::create(producer, GEAR_SNAPSHOT_TOPIC, nullptr, errstr);
    if (!this->gearSnapshotTopic) {
        std::cerr << "Failed to create topic: " << errstr << std::endl;
        exit(1);
    }
}

void MessagingMgr::InitAccountTopic()
{
    std::string errstr;

    /*
    * Create topic handle.
    */
    this->accountTopic = RdKafka::Topic::create(producer, ACCOUNT_TOPIC, nullptr, errstr);
    if (!this->accountTopic) {
        std::cerr << "Failed to create topic: " << errstr << std::endl;
        exit(1);
    }
}

void MessagingMgr::InitCharacterTopic()
{
    std::string errstr;

    /*
    * Create topic handle.
    */
    this->characterTopic = RdKafka::Topic::create(producer, CHARACTER_TOPIC, nullptr, errstr);
    if (!this->characterTopic) {
        std::cerr << "Failed to create topic: " << errstr << std::endl;
        exit(1);
    }
}

void MessagingMgr::InitGearPurchaseAckTopic()
{
    std::string errstr;

    /*
    * Create topic handle.
    */
    this->gearPurchaseAckTopic = RdKafka::Topic::create(producer, GEAR_PURCHASE_ACK_TOPIC, nullptr, errstr);
    if (!this->gearPurchaseAckTopic) {
        std::cerr << "Failed to create topic: " << errstr << std::endl;
        exit(1);
    }
}

std::string MessagingMgr::ConstructAccountSnapshot(uint32 accountId, std::string username, std::string hashedPassword) const
{
    std::string result;

    // account id
    result += std::to_string(accountId);
    result += '#';

    // username
    result += username;
    result += '#';

    // hashedPassword
    result += hashedPassword;

    return result;
}

void MessagingMgr::SendGearSnapshot(std::string message)
{
    RdKafka::ErrorCode resp = producer->produce(this->gearSnapshotTopic, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY, const_cast<char *>(message.c_str()), message.size(), NULL, NULL);
    if (resp != RdKafka::ERR_NO_ERROR)
        std::cerr << "% Produce failed: " << RdKafka::err2str(resp) << std::endl;
    else
        std::cerr << "% Produced message (" << message.size() << " bytes)" << std::endl;
}

void MessagingMgr::SendAccountSnapshot(uint32 accountId, std::string username, std::string hashedPassword)
{
    std::string message = ConstructAccountSnapshot(accountId, username, hashedPassword);
    RdKafka::ErrorCode resp = producer->produce(this->accountTopic, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY, const_cast<char *>(message.c_str()), message.size(), NULL, NULL);
    if (resp != RdKafka::ERR_NO_ERROR)
        std::cerr << "% Produce failed: " << RdKafka::err2str(resp) << std::endl;
    else
        std::cerr << "% Produced message (" << message.size() << " bytes)" << std::endl;
}

void MessagingMgr::SendCharacter(uint32 accountId, uint32 characterId, std::string characterName, uint8 characterClass, bool enabled)
{
    std::string message = std::to_string(accountId) + '#' + std::to_string(characterId) + '#' + characterName + '#' + std::to_string(characterClass) + '#' + (enabled ? '1' : '0');

    RdKafka::ErrorCode resp = producer->produce(this->characterTopic, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY, const_cast<char *>(message.c_str()), message.size(), NULL, NULL);
    if (resp != RdKafka::ERR_NO_ERROR)
        std::cerr << "% Produce failed: " << RdKafka::err2str(resp) << std::endl;
    else
        std::cerr << "% Produced message (" << message.size() << " bytes)" << std::endl;
}

void MessagingMgr::SendGearPurchaseAck(uint32 orderId, bool success)
{
    std::string message = std::to_string(orderId) + '#' + (success ? "true" : "false");

    RdKafka::ErrorCode resp = producer->produce(this->gearPurchaseAckTopic, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY, const_cast<char *>(message.c_str()), message.size(), NULL, NULL);
    if (resp != RdKafka::ERR_NO_ERROR)
        std::cerr << "% Produce failed: " << RdKafka::err2str(resp) << std::endl;
    else
        std::cerr << "% Produced message (" << message.size() << " bytes)" << std::endl;
}

void MessagingMgr::ConsumeGearPurchaseEvents()
{
    RdKafka::Message *msg = consumer->consume(0);
    HandleGearPurchaseMessage(*msg);
    delete msg;
}

void MessagingMgr::Update(uint32 diff)
{
    ConsumeGearPurchaseEvents();
    this->producer->poll(0);

    uint32 RESYNC_TIMER = 6 * 3600 * 1000; // @todo: externalize this property
    timeSinceLastResync += diff;
    if (timeSinceLastResync > RESYNC_TIMER)
    {
        ResyncMessages();
        timeSinceLastResync = 0;
    }
}

void MessagingMgr::ResyncMessages()
{
    std::cout << "Kafka Resync process started" << std::endl;
    SyncAccounts();
    SyncCharactes();
    SyncGearSnapshots();
    SyncGearPurchaseAcks();

    std::cout << "Kafka Resync process finished" << std::endl;
}

MessagingMgr* MessagingMgr::instance()
{
    static MessagingMgr instance;
    return &instance;
}
