
#include "MessagingMgr.h"
#include "rdkafkacpp.h"
#include <chrono>
#include <iostream>

MessagingMgr::MessagingMgr()
{
    
}

MessagingMgr::~MessagingMgr()
{
   
}

void MessagingMgr::Update()
{

}

MessagingMgr* MessagingMgr::instance()
{
    static MessagingMgr instance;
    return &instance;
}
