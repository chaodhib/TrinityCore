
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
    uint32 GetFirstParam(std::string payload);
    uint32 GetSecondParam(std::string payload);
};

class OffsetCommitCb : public RdKafka::OffsetCommitCb {
    void offset_commit_cb(RdKafka::ErrorCode err, std::vector<RdKafka::TopicPartition*>&offsets);
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
        void InitGearPurchaseAckTopic();
        void ConsumerSubscribe();

        std::string ConstructAccountSnapshot(uint32 accountId, std::string username, std::string hashedPassword) const;
        std::string ConstructCharacterSnapshot(uint32 accountId, uint32 characterId, std::string characterName, uint8 characterClass, bool enabled) const;

        std::pair<uint32, uint32> ParseItemQuantityMapEntry(const std::string st);
        bool IsValidItemQuantityEntry(const std::string st);
        bool ParseGearPurchaseMessage(std::string message, uint32 &orderId, uint32 &characterId, std::list<std::pair<uint32, uint32> > &itemQuantityList);
        void HandleGearPurchaseMessage(RdKafka::Message &msg);
        void SyncAccounts();
        void SyncCharactes();
        void SyncGearSnapshots();
        void SyncGearPurchaseAcks();

    public:
        static MessagingMgr* instance();
        void Update(uint32 diff);
        void SendGearSnapshot(std::string message);
        void SendAccountSnapshot(uint32 accountId, std::string username, std::string hashedPassword);
        void SendCharacter(uint32 accountId, uint32 characterId, std::string characterName, uint8 characterClass, bool enabled);
        void SendGearPurchaseAck(uint32 orderId, bool success);
        void ConsumeGearPurchaseEvents();

        void ResyncMessages();

        static const std::string CHARACTER_TOPIC;
        static const std::string ACCOUNT_TOPIC;
        static const std::string GEAR_SNAPSHOT_TOPIC;
        static const std::string GEAR_PURCHASE_TOPIC;
        static const std::string GEAR_PURCHASE_ACK_TOPIC;

    private:
        RdKafka::Producer *producer;
        RdKafka::KafkaConsumer *consumer;
        RdKafka::Topic *accountTopic;
        RdKafka::Topic *gearSnapshotTopic;
        RdKafka::Topic *characterTopic;
        RdKafka::Topic *gearPurchaseAckTopic;

        uint32 timeSinceLastResync = 0;

        DeliveryReportCb dr_cb;
        EventCb event_cb;
        OffsetCommitCb commit_cb;
};

#define sMessagingMgr MessagingMgr::instance()

#endif
