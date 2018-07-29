
#include "MessagingMgr.h"

MessagingMgr::MessagingMgr()
{
    InitProducer();
    InitGearTopic();
    InitAccountTopic();
    InitCharacterTopic();
}

MessagingMgr::~MessagingMgr()
{
    delete accountTopic;
    delete gearTopic;
    delete producer;
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

void MessagingMgr::Update()
{
    this->producer->poll(0);
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

MessagingMgr* MessagingMgr::instance()
{
    static MessagingMgr instance;
    return &instance;
}
