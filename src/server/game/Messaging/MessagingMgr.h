
#ifndef _MESSAGINGMGR_H
#define _MESSAGINGMGR_H

#include "Define.h"
#include "rdkafkacpp.h"
#include <chrono>
#include <iostream>

class ExampleEventCb : public RdKafka::EventCb {
public:
    void event_cb(RdKafka::Event &event) {
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
};

class ExampleDeliveryReportCb : public RdKafka::DeliveryReportCb {
public:
    void dr_cb(RdKafka::Message &message) {
        std::cout << "Message delivery for (" << message.len() << " bytes): " << message.errstr() << std::endl;

        if (message.err() == RdKafka::ErrorCode::ERR_NO_ERROR) {

            //if (message.topic_name() == "ACCOUNT") {
            //    std::string username = ParseAccountUsername(message.payload);
            //    PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_ACCOUNT_KAFKA_OK);
            //    stmt->setString(0, username);
            //    LoginDatabase.DirectExecute(stmt);
            //} else if (message.topic_name() == "CHARACTER") {
            //    std::string username = ParseAccountUsername(message.payload);
            //    PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_ACCOUNT_KAFKA_OK);
            //    stmt->setString(0, username);
            //    LoginDatabase.DirectExecute(stmt);
            //}
        }
    }
};

class TC_GAME_API MessagingMgr
{
    private:
        MessagingMgr();
        ~MessagingMgr();

        void InitProducer();
        void InitConsumer();
        void InitGearTopic();
        void InitAccountTopic();
        void InitCharacterTopic();
        void ConsumerSubscribe();

        void HandleGearPurchaseMessage(RdKafka::Message &msg);

    public:
        static MessagingMgr* instance();
        void Update();
        void SendGearSnapshot(std::string message);
        void SendAccountSnapshot(std::string message);
        void SendCharacter(std::string message);
        void ConsumeGearPurchaseEvents();

    private:
        RdKafka::Producer *producer;
        RdKafka::KafkaConsumer *consumer;
        RdKafka::Topic *accountTopic;
        RdKafka::Topic *gearTopic;
        RdKafka::Topic *characterTopic;
        RdKafka::Topic *gearPurchaseTopic;

        ExampleDeliveryReportCb ex_dr_cb;
        ExampleEventCb ex_event_cb;
};

#define sMessagingMgr MessagingMgr::instance()

#endif
