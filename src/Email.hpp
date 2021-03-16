#pragma once

#include "CNProtocol.hpp"
#include "CNStructs.hpp"
#include "CNShardServer.hpp"

#include "db/Database.hpp"
#include "PlayerManager.hpp"
#include "ItemManager.hpp"
#include "ChatManager.hpp"

namespace Email {
	void init();

	void emailUpdateCheck(CNSocket* sock, CNPacketData* data);
	void emailReceivePageList(CNSocket* sock, CNPacketData* data);
	void emailRead(CNSocket* sock, CNPacketData* data);
	void emailReceiveTaros(CNSocket* sock, CNPacketData* data);
	void emailReceiveItemSingle(CNSocket* sock, CNPacketData* data);
	void emailReceiveItemAll(CNSocket* sock, CNPacketData* data);
	void emailDelete(CNSocket* sock, CNPacketData* data);
	void emailSend(CNSocket* sock, CNPacketData* data);
}
