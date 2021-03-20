#include "Email.hpp"

#include "core/Core.hpp"
#include "servers/CNShardServer.hpp"

#include "db/Database.hpp"
#include "PlayerManager.hpp"
#include "Items.hpp"
#include "Chat.hpp"

using namespace Email;

// New email notification
static void emailUpdateCheck(CNSocket* sock, CNPacketData* data) {
    INITSTRUCT(sP_FE2CL_REP_PC_NEW_EMAIL, resp);
    resp.iNewEmailCnt = Database::getUnreadEmailCount(PlayerManager::getPlayer(sock)->iID);
    sock->sendPacket(resp, P_FE2CL_REP_PC_NEW_EMAIL);
}

// Retrieve page of emails
static void emailReceivePageList(CNSocket* sock, CNPacketData* data) {
    auto pkt = (sP_CL2FE_REQ_PC_RECV_EMAIL_PAGE_LIST*)data->buf;

    INITSTRUCT(sP_FE2CL_REP_PC_RECV_EMAIL_PAGE_LIST_SUCC, resp);
    resp.iPageNum = pkt->iPageNum;

    std::vector<Database::EmailData> emails = Database::getEmails(PlayerManager::getPlayer(sock)->iID, pkt->iPageNum);
    for (int i = 0; i < emails.size(); i++) {
        // convert each email and load them into the packet
        Database::EmailData* email = &emails.at(i);
        sEmailInfo* emailInfo = new sEmailInfo();
        emailInfo->iEmailIndex = email->MsgIndex;
        emailInfo->iReadFlag = email->ReadFlag;
        emailInfo->iItemCandyFlag = email->ItemFlag;
        emailInfo->iFromPCUID = email->SenderId;
        emailInfo->SendTime = timeStampToStruct(email->SendTime);
        emailInfo->DeleteTime = timeStampToStruct(email->DeleteTime);
        U8toU16(email->SenderFirstName, emailInfo->szFirstName, sizeof(emailInfo->szFirstName));
        U8toU16(email->SenderLastName, emailInfo->szLastName, sizeof(emailInfo->szLastName));
        U8toU16(email->SubjectLine, emailInfo->szSubject, sizeof(emailInfo->szSubject));
        resp.aEmailInfo[i] = *emailInfo;
    }

    sock->sendPacket(resp, P_FE2CL_REP_PC_RECV_EMAIL_PAGE_LIST_SUCC);
}

// Read individual email
static void emailRead(CNSocket* sock, CNPacketData* data) {
    auto pkt = (sP_CL2FE_REQ_PC_READ_EMAIL*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);

    Database::EmailData email = Database::getEmail(plr->iID, pkt->iEmailIndex);
    sItemBase* attachments = Database::getEmailAttachments(plr->iID, pkt->iEmailIndex);
    email.ReadFlag = 1; // mark as read
    Database::updateEmailContent(&email);

    INITSTRUCT(sP_FE2CL_REP_PC_READ_EMAIL_SUCC, resp);
    resp.iEmailIndex = pkt->iEmailIndex;
    resp.iCash = email.Taros;
    for (int i = 0; i < 4; i++) {
        resp.aItem[i] = attachments[i];
    }
    U8toU16(email.MsgBody, (char16_t*)resp.szContent, sizeof(resp.szContent));

    sock->sendPacket(resp, P_FE2CL_REP_PC_READ_EMAIL_SUCC);
}

// Retrieve attached taros from email
static void emailReceiveTaros(CNSocket* sock, CNPacketData* data) {
    auto pkt = (sP_CL2FE_REQ_PC_RECV_EMAIL_CANDY*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);

    Database::EmailData email = Database::getEmail(plr->iID, pkt->iEmailIndex);
    // money transfer
    plr->money += email.Taros;
    email.Taros = 0;
    // update Taros in email
    Database::updateEmailContent(&email);

    INITSTRUCT(sP_FE2CL_REP_PC_RECV_EMAIL_CANDY_SUCC, resp);
    resp.iCandy = plr->money;
    resp.iEmailIndex = pkt->iEmailIndex;

    sock->sendPacket(resp, P_FE2CL_REP_PC_RECV_EMAIL_CANDY_SUCC);
}

// Retrieve individual attached item from email
static void emailReceiveItemSingle(CNSocket* sock, CNPacketData* data) {
    auto pkt = (sP_CL2FE_REQ_PC_RECV_EMAIL_ITEM*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    if (pkt->iSlotNum < 0 || pkt->iSlotNum >= AINVEN_COUNT || pkt->iSlotNum < 1 || pkt->iSlotNum > 4)
        return; // sanity check

    // get email item from db and delete it
    sItemBase* attachments = Database::getEmailAttachments(plr->iID, pkt->iEmailIndex);
    sItemBase itemFrom = attachments[pkt->iEmailItemSlot - 1];
    Database::deleteEmailAttachments(plr->iID, pkt->iEmailIndex, pkt->iEmailItemSlot);

    // move item to player inventory
    sItemBase& itemTo = plr->Inven[pkt->iSlotNum];
    itemTo.iID = itemFrom.iID;
    itemTo.iOpt = itemFrom.iOpt;
    itemTo.iTimeLimit = itemFrom.iTimeLimit;
    itemTo.iType = itemFrom.iType;

    INITSTRUCT(sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_SUCC, resp);
    resp.iEmailIndex = pkt->iEmailIndex;
    resp.iEmailItemSlot = pkt->iEmailItemSlot;
    resp.iSlotNum = pkt->iSlotNum;

    sock->sendPacket(resp, P_FE2CL_REP_PC_RECV_EMAIL_ITEM_SUCC);

    // update inventory
    INITSTRUCT(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC, resp2);
    resp2.eIL = 1;
    resp2.iSlotNum = resp.iSlotNum;
    resp2.Item = itemTo;

    sock->sendPacket(resp2, P_FE2CL_REP_PC_GIVE_ITEM_SUCC);
}

// Retrieve all attached items from email
static void emailReceiveItemAll(CNSocket* sock, CNPacketData* data) {
    auto pkt = (sP_CL2FE_REQ_PC_RECV_EMAIL_ITEM_ALL*)data->buf;

    // move items to player inventory
    Player* plr = PlayerManager::getPlayer(sock);
    sItemBase* itemsFrom = Database::getEmailAttachments(plr->iID, pkt->iEmailIndex);
    for (int i = 0; i < 4; i++) {
        int slot = Items::findFreeSlot(plr);
        if (slot < 0 || slot >= AINVEN_COUNT) {
            INITSTRUCT(sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_ALL_FAIL, failResp);
            failResp.iEmailIndex = pkt->iEmailIndex;
            failResp.iErrorCode = 0; // ???
            break; // sanity check; should never happen
        }

        // copy data over
        sItemBase itemFrom = itemsFrom[i];
        sItemBase& itemTo = plr->Inven[slot];
        itemTo.iID = itemFrom.iID;
        itemTo.iOpt = itemFrom.iOpt;
        itemTo.iTimeLimit = itemFrom.iTimeLimit;
        itemTo.iType = itemFrom.iType;

        // update inventory
        INITSTRUCT(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC, resp2);
        resp2.eIL = 1;
        resp2.iSlotNum = slot;
        resp2.Item = itemTo;

        sock->sendPacket(resp2, P_FE2CL_REP_PC_GIVE_ITEM_SUCC);
    }

    // delete all items from db
    Database::deleteEmailAttachments(plr->iID, pkt->iEmailIndex, -1);

    INITSTRUCT(sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_ALL_SUCC, resp);
    resp.iEmailIndex = pkt->iEmailIndex;

    sock->sendPacket(resp, P_FE2CL_REP_PC_RECV_EMAIL_ITEM_ALL_SUCC);
}

// Delete an email
static void emailDelete(CNSocket* sock, CNPacketData* data) {
    auto pkt = (sP_CL2FE_REQ_PC_DELETE_EMAIL*)data->buf;

    Database::deleteEmails(PlayerManager::getPlayer(sock)->iID, pkt->iEmailIndexArray);

    INITSTRUCT(sP_FE2CL_REP_PC_DELETE_EMAIL_SUCC, resp);
    for (int i = 0; i < 5; i++) {
        resp.iEmailIndexArray[i] = pkt->iEmailIndexArray[i]; // i'm scared of memcpy
    }

    sock->sendPacket(resp, P_FE2CL_REP_PC_DELETE_EMAIL_SUCC);
}

// Send an email
static void emailSend(CNSocket* sock, CNPacketData* data) {
    auto pkt = (sP_CL2FE_REQ_PC_SEND_EMAIL*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    // sanity checks
    bool invalid = false;
    int itemCount = 0;

    std::set<int> seen;
    for (int i = 0; i < 4; i++) {
        int slot = pkt->aItem[i].iSlotNum;
        if (slot < 0 || slot >= AINVEN_COUNT) {
            invalid = true;
            break;
        }

        sItemBase* item = &pkt->aItem[i].ItemInven;
        sItemBase* real = &plr->Inven[slot];

        if (item->iID == 0)
            continue;

        // was the same item added multiple times?
        if (seen.count(slot) > 0) {
            invalid = true;
            break;
        }
        seen.insert(slot);

        itemCount++;
        if (item->iType != real->iType || item->iID != real->iID
            || item->iOpt <= 0 || item->iOpt > real->iOpt) {
            invalid = true;
            break;
        }
    }

    if (pkt->iCash < 0 || pkt->iCash > plr->money + 50 + 20 * itemCount || invalid) {
        INITSTRUCT(sP_FE2CL_REP_PC_SEND_EMAIL_FAIL, errResp);
        errResp.iErrorCode = 1;
        errResp.iTo_PCUID = pkt->iTo_PCUID;
        sock->sendPacket(errResp, P_FE2CL_REP_PC_SEND_EMAIL_FAIL);
        return;
    }

    INITSTRUCT(sP_FE2CL_REP_PC_SEND_EMAIL_SUCC, resp);

    if (pkt->iCash || pkt->aItem[0].ItemInven.iID) {
        // if there are item or taro attachments
        Player otherPlr = {};
        Database::getPlayer(&otherPlr, pkt->iTo_PCUID);
        if (otherPlr.iID != 0 && plr->PCStyle2.iPayzoneFlag != otherPlr.PCStyle2.iPayzoneFlag) {
            // if the players are not in the same time period
            INITSTRUCT(sP_FE2CL_REP_PC_SEND_EMAIL_FAIL, resp);
            resp.iErrorCode = 9; // error code 9 tells the player they can't send attachments across time
            resp.iTo_PCUID = pkt->iTo_PCUID;
            sock->sendPacket(resp, P_FE2CL_REP_PC_SEND_EMAIL_FAIL);
            return;
        }
    }

    // handle items
    std::vector<sItemBase> attachments;
    std::vector<int> attSlots;
    for (int i = 0; i < 4; i++) {
        sEmailItemInfoFromCL attachment = pkt->aItem[i];

        // skip empty slots
        if (attachment.ItemInven.iID == 0)
            continue;

        resp.aItem[i] = attachment;
        attachments.push_back(attachment.ItemInven);
        attSlots.push_back(attachment.iSlotNum);
        // delete item
        plr->Inven[attachment.iSlotNum] = { 0, 0, 0, 0 };
    }

    int cost = pkt->iCash + 50 + 20 * attachments.size(); // attached taros + postage
    plr->money -= cost;
    Database::EmailData email = {
        (int)pkt->iTo_PCUID, // PlayerId
        Database::getNextEmailIndex(pkt->iTo_PCUID), // MsgIndex
        0, // ReadFlag (unread)
        (pkt->iCash > 0 || attachments.size() > 0) ? 1 : 0, // ItemFlag
        plr->iID, // SenderID
        AUTOU16TOU8(plr->PCStyle.szFirstName), // SenderFirstName
        AUTOU16TOU8(plr->PCStyle.szLastName), // SenderLastName
        Chat::sanitizeText(AUTOU16TOU8(pkt->szSubject)), // SubjectLine
        Chat::sanitizeText(AUTOU16TOU8(pkt->szContent), true), // MsgBody
        pkt->iCash, // Taros
        (uint64_t)getTimestamp(), // SendTime
        0 // DeleteTime (unimplemented)
    };

    if (!Database::sendEmail(&email, attachments)) {
        plr->money += cost; // give money back
        // give items back
        while (!attachments.empty()) {
            sItemBase attachment = attachments.back();
            plr->Inven[attSlots.back()] = attachment;

            attachments.pop_back();
            attSlots.pop_back();
        }

        // send error message
        INITSTRUCT(sP_FE2CL_REP_PC_SEND_EMAIL_FAIL, errResp);
        errResp.iErrorCode = 1;
        errResp.iTo_PCUID = pkt->iTo_PCUID;
        sock->sendPacket(errResp, P_FE2CL_REP_PC_SEND_EMAIL_FAIL);
        return;
    }

    // HACK: use set value packet to force GUI taros update
    INITSTRUCT(sP_FE2CL_GM_REP_PC_SET_VALUE, tarosResp);
    tarosResp.iPC_ID = plr->iID;
    tarosResp.iSetValueType = 5;
    tarosResp.iSetValue = plr->money;
    sock->sendPacket(tarosResp, P_FE2CL_GM_REP_PC_SET_VALUE);

    resp.iCandy = plr->money;
    resp.iTo_PCUID = pkt->iTo_PCUID;

    sock->sendPacket(resp, P_FE2CL_REP_PC_SEND_EMAIL_SUCC);
}

void Email::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_EMAIL_UPDATE_CHECK, emailUpdateCheck);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_RECV_EMAIL_PAGE_LIST, emailReceivePageList);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_READ_EMAIL, emailRead);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_RECV_EMAIL_CANDY, emailReceiveTaros);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_RECV_EMAIL_ITEM, emailReceiveItemSingle);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_RECV_EMAIL_ITEM_ALL, emailReceiveItemAll);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_DELETE_EMAIL, emailDelete);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_SEND_EMAIL, emailSend);
}
