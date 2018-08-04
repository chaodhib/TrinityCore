
#ifndef _MESSAGINGMGR_H
#define _MESSAGINGMGR_H

#include "Define.h"
#include "rdkafkacpp.h"

class EventCb : public RdKafka::EventCb {
public:
    void event_cb(RdKafka::Event &event);
};

class DeliveryReportCb : public RdKafka::DeliveryReportCb {
public:
    void dr_cb(RdKafka::Message &message);
    uint32 GetIdFromAccountEvent(std::string payload);
    uint32 GetIdFromCharacterEvent(std::string payload);
    uint32 GetIdFromGearSnapshotEvent(std::string payload);
};

class TC_GAME_API MessagingMgr
{
    private:
        MessagingMgr();
        ~MessagingMgr();

        void InitProducer();
        void InitConsumer();
        void InitGearSnapshotTopic();
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

        static const std::string CHARACTER_TOPIC;
        static const std::string ACCOUNT_TOPIC;
        static const std::string GEAR_SNAPSHOT_TOPIC;
        static const std::string GEAR_PURCHASE_TOPIC;

    private:
        RdKafka::Producer *producer;
        RdKafka::KafkaConsumer *consumer;
        RdKafka::Topic *accountTopic;
        RdKafka::Topic *gearSnapshotTopic;
        RdKafka::Topic *characterTopic;
        RdKafka::Topic *gearPurchaseTopic;

        DeliveryReportCb dr_cb;
        EventCb event_cb;
};

#define sMessagingMgr MessagingMgr::instance()

#endif
