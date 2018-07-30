
#include "MessagingMgr.h"
#include "ShopMgr.h"

static int verbosity = 3;

void GearPurchaseConsumeCb::consume_cb(RdKafka::Message &msg, void *opaque) {

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

        sShopMgr->HandlePurchaseOrder(std::string(static_cast<const char *>(msg.payload())));

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

MessagingMgr::MessagingMgr()
{
    InitProducer();
    InitConsumer();
    InitGearTopic();
    InitAccountTopic();
    InitCharacterTopic();
    ConsumerSubscribe();
}

MessagingMgr::~MessagingMgr()
{
    consumer->close();

    delete accountTopic;
    delete gearTopic;
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
    conf->set("event_cb", &ex_event_cb, errstr);
    conf->set("dr_cb", &ex_dr_cb, errstr);

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

    if (conf->set("auto.commit.interval.ms ", "30000", errstr) != RdKafka::Conf::CONF_OK) {
        std::cerr << errstr << std::endl;
        exit(1);
    }
    
    if (conf->set("consume_cb", &gearPurchaseConsumerCb, errstr) != RdKafka::Conf::CONF_OK) {
        std::cerr << errstr << std::endl;
        exit(1);
    }

    conf->set("event_cb", &ex_event_cb, errstr);
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

void MessagingMgr::InitGearTopic()
{
    std::string topic_str = "GEAR_SNAPSHOT";
    std::string errstr;

    /*
    * Create topic handle.
    */
    this->gearTopic = RdKafka::Topic::create(producer, topic_str, nullptr, errstr);
    if (!this->gearTopic) {
        std::cerr << "Failed to create topic: " << errstr << std::endl;
        exit(1);
    }
}

void MessagingMgr::InitAccountTopic()
{
    std::string topic_str = "ACCOUNT_SNAPSHOT";
    std::string errstr;

    /*
    * Create topic handle.
    */
    this->accountTopic = RdKafka::Topic::create(producer, topic_str, nullptr, errstr);
    if (!this->accountTopic) {
        std::cerr << "Failed to create topic: " << errstr << std::endl;
        exit(1);
    }
}

void MessagingMgr::InitCharacterTopic()
{
    std::string topic_str = "CHARACTER";
    std::string errstr;

    /*
    * Create topic handle.
    */
    this->characterTopic = RdKafka::Topic::create(producer, topic_str, nullptr, errstr);
    if (!this->characterTopic) {
        std::cerr << "Failed to create topic: " << errstr << std::endl;
        exit(1);
    }
}

void MessagingMgr::SendGearSnapshot(std::string message)
{
    RdKafka::ErrorCode resp = producer->produce(this->gearTopic, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY, const_cast<char *>(message.c_str()), message.size(), NULL, NULL);
    if (resp != RdKafka::ERR_NO_ERROR)
        std::cerr << "% Produce failed: " << RdKafka::err2str(resp) << std::endl;
    else
        std::cerr << "% Produced message (" << message.size() << " bytes)" << std::endl;
}

void MessagingMgr::SendAccountSnapshot(std::string message)
{
    RdKafka::ErrorCode resp = producer->produce(this->accountTopic, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY, const_cast<char *>(message.c_str()), message.size(), NULL, NULL);
    if (resp != RdKafka::ERR_NO_ERROR)
        std::cerr << "% Produce failed: " << RdKafka::err2str(resp) << std::endl;
    else
        std::cerr << "% Produced message (" << message.size() << " bytes)" << std::endl;
}

void MessagingMgr::SendCharacter(std::string message)
{
    RdKafka::ErrorCode resp = producer->produce(this->characterTopic, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY, const_cast<char *>(message.c_str()), message.size(), NULL, NULL);
    if (resp != RdKafka::ERR_NO_ERROR)
        std::cerr << "% Produce failed: " << RdKafka::err2str(resp) << std::endl;
    else
        std::cerr << "% Produced message (" << message.size() << " bytes)" << std::endl;
}

void MessagingMgr::ConsumeGearPurchaseEvents()
{
    RdKafka::Message *msg = consumer->consume(0);
    gearPurchaseConsumerCb.consume_cb(*msg, nullptr);
    delete msg;
}

void MessagingMgr::Update()
{
    ConsumeGearPurchaseEvents();
    this->producer->poll(0);
}

MessagingMgr* MessagingMgr::instance()
{
    static MessagingMgr instance;
    return &instance;
}
