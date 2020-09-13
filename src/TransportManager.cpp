#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "PlayerManager.hpp"
#include "TransportManager.hpp"

void TransportManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_REGIST_TRANSPORTATION_LOCATION, transportRegisterLocationHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_WARP_USE_TRANSPORTATION, transportWarpHandler);
}

void TransportManager::transportRegisterLocationHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_REGIST_TRANSPORTATION_LOCATION))
        return; // malformed packet
    
    sP_CL2FE_REQ_REGIST_TRANSPORTATION_LOCATION* transport = (sP_CL2FE_REQ_REGIST_TRANSPORTATION_LOCATION*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    std::cout << "request to register transport, eTT " << transport->eTT << ", locID " << transport->iLocationID << ", npc " << transport->iNPC_ID << std::endl;
    if (transport->eTT == 1) { // S.C.A.M.P.E.R.
        int32_t base = 1;
        if (transport->iLocationID < 1 || transport->iLocationID > 31) { // sanity check
            std::cout << "[WARN] S.C.A.M.P.E.R. location ID " << transport->iLocationID << " is out of bounds" << std::endl;
            INITSTRUCT(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL, failResp);
            failResp.eTT = transport->eTT;
            failResp.iErrorCode = 0; // TODO: review what error code to use here
            failResp.iLocationID = transport->iLocationID;
            sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL, sizeof(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL));
            return;
        }
        // update registration bitfield using bit shifting + bitwise or
        plr->iWarpLocationFlag |= (base << (transport->iLocationID - 1));
    }
    else if (transport->eTT == 2) { // Monkey Skyway System
        int64_t base = 1;
        if (transport->iLocationID < 1 || transport->iLocationID > 127) { // sanity check
            std::cout << "[WARN] Skyway location ID " << transport->iLocationID << " is out of bounds" << std::endl;
            INITSTRUCT(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL, failResp);
            failResp.eTT = transport->eTT;
            failResp.iErrorCode = 0; // TODO: review what error code to use here
            failResp.iLocationID = transport->iLocationID;
            sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL, sizeof(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL));
            return;
        }
        /* 
        * assuming the two bitfields are just stuck together to make a longer one... do a similar operation, but on the respective integer
        * this approach seems to work with initial testing, but we have yet to see a monkey ID greater than 63.
        */
        plr->aSkywayLocationFlag[transport->iLocationID > 63 ? 1 : 0] |= (base << (transport->iLocationID > 63 ? transport->iLocationID - 65 : transport->iLocationID - 1));
    }
    else {
        std::cout << "[WARN] Unknown mode of transport; eTT = " << transport->eTT << std::endl;
        INITSTRUCT(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL, failResp);
        failResp.eTT = transport->eTT;
        failResp.iErrorCode = 0; // TODO: review what error code to use here
        failResp.iLocationID = transport->iLocationID;
        sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL, sizeof(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL));
        return;
    }

    INITSTRUCT(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_SUCC, resp);

    // response parameters
    resp.eTT = transport->eTT;
    resp.iLocationID = transport->iLocationID;
    resp.iWarpLocationFlag = plr->iWarpLocationFlag;
    resp.aWyvernLocationFlag[0] = plr->aSkywayLocationFlag[0];
    resp.aWyvernLocationFlag[1] = plr->aSkywayLocationFlag[1];

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_SUCC, sizeof(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_SUCC));
}

void TransportManager::transportWarpHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_WARP_USE_TRANSPORTATION))
        return; // malformed packet

    sP_CL2FE_REQ_PC_WARP_USE_TRANSPORTATION* req = (sP_CL2FE_REQ_PC_WARP_USE_TRANSPORTATION*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    /*
    * req:
    * eIL -- the location ID of the desination
    * iNPC_ID -- the ID of the NPC who is warping you
    * iTransporationID -- ??????
    * iSlotNum -- inventory slot number; why??
    */

    int cost = 100; // TODO: lookup correct fare

    if (plr->money < cost || true) { // future sanity check, just fail for now
        INITSTRUCT(sP_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_FAIL, failResp);
        failResp.iErrorCode = 0; // TODO: error code
        failResp.iTransportationID = req->iTransporationID;
        sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_FAIL, sizeof(sP_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_FAIL));
        return;
    }

    plr->money -= cost;

    INITSTRUCT(sP_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_SUCC, resp);

    // response parameters
    //resp.eTT =
    resp.iCandy = plr->money;
    resp.iX = plr->x;
    resp.iY = plr->y;
    resp.iZ = plr->z;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_SUCC, sizeof(sP_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_SUCC));
}
