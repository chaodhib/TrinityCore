
#ifndef _MESSAGINGMGR_H
#define _MESSAGINGMGR_H

class TC_GAME_API MessagingMgr
{
    private:
        MessagingMgr();
        ~MessagingMgr();

    public:
        static MessagingMgr* instance();
        void Update();
};

#define sMessagingMgr MessagingMgr::instance()

#endif
