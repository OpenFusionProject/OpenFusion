#include "Chunking.hpp"

#include "Player.hpp"
#include "MobAI.hpp"
#include "NPCManager.hpp"

#include <assert.h>

using namespace Chunking;

/*
 * The initial chunkPos value before a player is placed into the world.
 */
const ChunkPos Chunking::INVALID_CHUNK = {};
constexpr size_t MAX_PC_PER_AROUND = (CN_PACKET_BUFFER_SIZE - 4) / sizeof(sPCAppearanceData);
constexpr size_t MAX_NPC_PER_AROUND = (CN_PACKET_BUFFER_SIZE - 4) / sizeof(sNPCAppearanceData);
constexpr size_t MAX_SHINY_PER_AROUND = (CN_PACKET_BUFFER_SIZE - 4) / sizeof(sShinyAppearanceData);
constexpr size_t MAX_TRANSPORTATION_PER_AROUND = (CN_PACKET_BUFFER_SIZE - 4) / sizeof(sTransportationAppearanceData);
constexpr size_t MAX_IDS_PER_AROUND_DEL = (CN_PACKET_BUFFER_SIZE - 4) / sizeof(int32_t);

std::map<ChunkPos, Chunk*> Chunking::chunks;

static void newChunk(ChunkPos pos) {
    if (chunkExists(pos)) {
        std::cout << "[WARN] Tried to create a chunk that already exists" << std::endl;
        return;
    }

    Chunk *chunk = new Chunk();
    chunks[pos] = chunk;

    // add the chunk to the cache of all players and NPCs in the surrounding chunks
    std::set<Chunk*> surroundings = getViewableChunks(pos);
    for (Chunk* c : surroundings)
        for (const EntityRef ref : c->entities)
            ref.getEntity()->viewableChunks.insert(chunk);
}

static void deleteChunk(ChunkPos pos) {
    if (!chunkExists(pos)) {
        std::cout << "[WARN] Tried to delete a chunk that doesn't exist" << std::endl;
        return;
    }

    Chunk* chunk = chunks[pos];

    // remove the chunk from the cache of all players and NPCs in the surrounding chunks
    std::set<Chunk*> surroundings = getViewableChunks(pos);
    for(Chunk* c : surroundings)
        for (const EntityRef ref : c->entities)
            ref.getEntity()->viewableChunks.erase(chunk);

    chunks.erase(pos); // remove from map
    delete chunk; // free from memory
}

void Chunking::trackEntity(ChunkPos chunkPos, const EntityRef ref) {
    if (!chunkExists(chunkPos))
        return; // shouldn't happen

    chunks[chunkPos]->entities.insert(ref);

    if (ref.kind == EntityKind::PLAYER)
        chunks[chunkPos]->nplayers++;
}

void Chunking::untrackEntity(ChunkPos chunkPos, const EntityRef ref) {
    if (!chunkExists(chunkPos))
        return; // do nothing if chunk doesn't even exist

    Chunk* chunk = chunks[chunkPos];

    chunk->entities.erase(ref); // gone

    if (ref.kind == EntityKind::PLAYER)
        chunks[chunkPos]->nplayers--;
    assert(chunks[chunkPos]->nplayers >= 0);

    // if chunk is completely empty, free it
    if (chunk->entities.size() == 0)
        deleteChunk(chunkPos);
}

template<class T>
static void sendAroundPacket(const EntityRef recipient, std::vector<std::vector<T>>& slices, size_t maxCnt, uint32_t packetId) {
    assert(recipient.kind == EntityKind::PLAYER);

    uint8_t pktBuf[CN_PACKET_BUFFER_SIZE];
    for (const auto& slice : slices) {
        memset(pktBuf, 0, CN_PACKET_BUFFER_SIZE);
        int count = slice.size();
        assert(count <= maxCnt);
        *((int32_t*)pktBuf) = count;
        T* data = (T*)(pktBuf + sizeof(int32_t));
        for (size_t i = 0; i < count; i++) {
            data[i] = slice[i];
        }
        recipient.sock->sendPacket(pktBuf, packetId, sizeof(int32_t) + (count * sizeof(T)));
    }
}

static void sendAroundDelPacket(const EntityRef recipient, std::vector<std::vector<int32_t>>& slices, bool isTransportation, uint32_t packetId) {
    assert(recipient.kind == EntityKind::PLAYER);

    size_t maxCnt = MAX_IDS_PER_AROUND_DEL;
    if (isTransportation)
        maxCnt -= 1; // account for eTT. sad.

    uint8_t pktBuf[CN_PACKET_BUFFER_SIZE];
    for (const auto& slice : slices) {
        memset(pktBuf, 0, CN_PACKET_BUFFER_SIZE);
        int count = slice.size();
        assert(count <= maxCnt);

        size_t baseSize;
        if (isTransportation) {
            sP_FE2CL_AROUND_DEL_TRANSPORTATION* pkt = (sP_FE2CL_AROUND_DEL_TRANSPORTATION*)pktBuf;
            pkt->eTT = 3;
            pkt->iCnt = count;
            baseSize = sizeof(sP_FE2CL_AROUND_DEL_TRANSPORTATION);
        } else {
            *((int32_t*)pktBuf) = count;
            baseSize = sizeof(int32_t);
        }
        int32_t* ids = (int32_t*)(pktBuf + baseSize);

        for (size_t i = 0; i < count; i++) {
            ids[i] = slice[i];
        }
        recipient.sock->sendPacket(pktBuf, packetId, baseSize + (count * sizeof(int32_t)));
    }
}

template<class T>
static void bufferAppearanceData(std::vector<std::vector<T>>& slices, const T& data, size_t maxCnt) {
    if (slices.empty())
        slices.push_back(std::vector<T>());
    std::vector<T>& slice = slices[slices.size() - 1];
    slice.push_back(data);
    if (slice.size() == maxCnt)
        slices.push_back(std::vector<T>());
}

static void bufferIdForDisappearance(std::vector<std::vector<int32_t>>& slices, int32_t id, size_t maxCnt) {
    if (slices.empty())
        slices.push_back(std::vector<int32_t>());
    std::vector<int32_t>& slice = slices[slices.size() - 1];
    slice.push_back(id);
    if (slice.size() == maxCnt)
        slices.push_back(std::vector<int32_t>());
}

void Chunking::addEntityToChunks(std::set<Chunk*> chnks, const EntityRef ref) {
    Entity *ent = ref.getEntity();
    bool alive = ent->isExtant();

    std::vector<std::vector<sPCAppearanceData>> pcAppearances;
    std::vector<std::vector<sNPCAppearanceData>> npcAppearances;
    std::vector<std::vector<sShinyAppearanceData>> shinyAppearances;
    std::vector<std::vector<sTransportationAppearanceData>> transportationAppearances;
    for (Chunk *chunk : chnks) {
        for (const EntityRef otherRef : chunk->entities) {
            // skip oneself
            if (ref == otherRef)
                continue;

            Entity *other = otherRef.getEntity();

            // notify all visible players of the existence of this Entity
            if (alive && otherRef.kind == EntityKind::PLAYER) {
                ent->enterIntoViewOf(otherRef.sock);
            }

            // notify this *player* of the existence of all visible Entities
            if (ref.kind == EntityKind::PLAYER && other->isExtant()) {
                sPCAppearanceData pcData;
                sNPCAppearanceData npcData;
                sShinyAppearanceData eggData;
                sTransportationAppearanceData busData;
                switch(otherRef.kind)
                {
                case EntityKind::PLAYER:
                    pcData = dynamic_cast<Player*>(other)->getAppearanceData();
                    bufferAppearanceData(pcAppearances, pcData, MAX_PC_PER_AROUND);
                    break;
                case EntityKind::SIMPLE_NPC:
                    npcData = dynamic_cast<BaseNPC*>(other)->getAppearanceData();
                    bufferAppearanceData(npcAppearances, npcData, MAX_NPC_PER_AROUND);
                    break;
                case EntityKind::COMBAT_NPC:
                    npcData = dynamic_cast<CombatNPC*>(other)->getAppearanceData();
                    bufferAppearanceData(npcAppearances, npcData, MAX_NPC_PER_AROUND);
                    break;
                case EntityKind::MOB:
                    npcData = dynamic_cast<Mob*>(other)->getAppearanceData();
                    bufferAppearanceData(npcAppearances, npcData, MAX_NPC_PER_AROUND);
                    break;
                case EntityKind::EGG:
                    eggData = dynamic_cast<Egg*>(other)->getShinyAppearanceData();
                    bufferAppearanceData(shinyAppearances, eggData, MAX_SHINY_PER_AROUND);
                    break;
                case EntityKind::BUS:
                    busData = dynamic_cast<Bus*>(other)->getTransportationAppearanceData();
                    bufferAppearanceData(transportationAppearances, busData, MAX_TRANSPORTATION_PER_AROUND);
                    break;
                default:
                    break;
                }
            }

            // for mobs, increment playersInView
            if (ref.kind == EntityKind::MOB && otherRef.kind == EntityKind::PLAYER)
                ((Mob*)ent)->playersInView++;
            if (otherRef.kind == EntityKind::MOB && ref.kind == EntityKind::PLAYER)
                ((Mob*)other)->playersInView++;
        }
    }

    if (ref.kind != EntityKind::PLAYER)
        return; // nothing to send

    if (!pcAppearances.empty())
        sendAroundPacket(ref, pcAppearances, MAX_PC_PER_AROUND, P_FE2CL_PC_AROUND);
    if (!npcAppearances.empty())
        sendAroundPacket(ref, npcAppearances, MAX_NPC_PER_AROUND, P_FE2CL_NPC_AROUND);
    if (!shinyAppearances.empty())
        sendAroundPacket(ref, shinyAppearances, MAX_SHINY_PER_AROUND, P_FE2CL_SHINY_AROUND);
    if (!transportationAppearances.empty())
        sendAroundPacket(ref, transportationAppearances, MAX_TRANSPORTATION_PER_AROUND, P_FE2CL_TRANSPORTATION_AROUND);
}

void Chunking::removeEntityFromChunks(std::set<Chunk*> chnks, const EntityRef ref) {
    Entity *ent = ref.getEntity();
    bool alive = ent->isExtant();

    std::vector<std::vector<int32_t>> pcDisappearances;
    std::vector<std::vector<int32_t>> npcDisappearances;
    std::vector<std::vector<int32_t>> shinyDisappearances;
    std::vector<std::vector<int32_t>> transportationDisappearances;
    for (Chunk *chunk : chnks) {
        for (const EntityRef otherRef : chunk->entities) {
            // skip oneself
            if (ref == otherRef)
                continue;

            Entity *other = otherRef.getEntity();

            // notify all visible players of the departure of this Entity
            if (alive && otherRef.kind == EntityKind::PLAYER) {
                ent->disappearFromViewOf(otherRef.sock);
            }

            // notify this *player* of the departure of all visible Entities
            if (ref.kind == EntityKind::PLAYER && other->isExtant()) {
                int32_t id;
                switch(otherRef.kind)
                {
                case EntityKind::PLAYER:
                    id = dynamic_cast<Player*>(other)->iID;
                    bufferIdForDisappearance(pcDisappearances, id, MAX_IDS_PER_AROUND_DEL);
                    break;
                case EntityKind::SIMPLE_NPC:
                case EntityKind::COMBAT_NPC:
                case EntityKind::MOB:
                    id = dynamic_cast<BaseNPC*>(other)->id;
                    bufferIdForDisappearance(npcDisappearances, id, MAX_IDS_PER_AROUND_DEL);
                    break;
                case EntityKind::EGG:
                    id = dynamic_cast<Egg*>(other)->id;
                    bufferIdForDisappearance(shinyDisappearances, id, MAX_IDS_PER_AROUND_DEL);
                    break;
                case EntityKind::BUS:
                    id = dynamic_cast<Bus*>(other)->id;
                    bufferIdForDisappearance(transportationDisappearances, id, MAX_IDS_PER_AROUND_DEL - 1);
                    break;
                default:
                    break;
                }
            }

            // for mobs, decrement playersInView
            if (ref.kind == EntityKind::MOB && otherRef.kind == EntityKind::PLAYER)
                ((Mob*)ent)->playersInView--;
            if (otherRef.kind == EntityKind::MOB && ref.kind == EntityKind::PLAYER)
                ((Mob*)other)->playersInView--;
        }
    }

    if (ref.kind != EntityKind::PLAYER)
        return; // nothing to send

    if (!pcDisappearances.empty())
        sendAroundDelPacket(ref, pcDisappearances, false, P_FE2CL_AROUND_DEL_PC);
    if (!npcDisappearances.empty())
        sendAroundDelPacket(ref, npcDisappearances, false, P_FE2CL_AROUND_DEL_NPC);
    if (!shinyDisappearances.empty())
        sendAroundDelPacket(ref, shinyDisappearances, false, P_FE2CL_AROUND_DEL_SHINY);
    if (!transportationDisappearances.empty())
        sendAroundDelPacket(ref, transportationDisappearances, true, P_FE2CL_AROUND_DEL_TRANSPORTATION);
}

static void emptyChunk(ChunkPos chunkPos) {
    if (!chunkExists(chunkPos)) {
        std::cout << "[WARN] Tried to empty chunk that doesn't exist\n";
        return; // chunk doesn't exist, we don't need to do anything
    }

    Chunk* chunk = chunks[chunkPos];

    if (chunk->nplayers > 0) {
        std::cout << "[WARN] Tried to empty chunk that still had players\n";
        return; // chunk doesn't exist, we don't need to do anything
    }

    // unspawn all of the mobs/npcs
    std::set refs(chunk->entities);
    for (const EntityRef ref : refs) {
        if (ref.kind == EntityKind::PLAYER)
            assert(0);

        // every call of this will check if the chunk is empty and delete it if so
        NPCManager::destroyNPC(ref.id);
    }
}

void Chunking::updateEntityChunk(const EntityRef ref, ChunkPos from, ChunkPos to) {
    Entity* ent = ref.getEntity();

    // move to other chunk's player set
    untrackEntity(from, ref); // this will delete the chunk if it's empty

    // if the new chunk doesn't exist, make it first
    if (!chunkExists(to))
        newChunk(to);

    trackEntity(to, ref);

    // calculate viewable chunks from both points
    std::set<Chunk*> oldViewables = getViewableChunks(from);
    std::set<Chunk*> newViewables = getViewableChunks(to);
    std::set<Chunk*> toExit, toEnter;

    /*
     * Calculate diffs. This is done to prevent phasing on chunk borders.
     * toExit will contain old viewables - new viewables, so the player will only be exited in chunks that are out of sight.
     * toEnter contains the opposite: new viewables - old viewables, chunks where we previously weren't visible from before.
     */
    std::set_difference(oldViewables.begin(), oldViewables.end(), newViewables.begin(), newViewables.end(),
        std::inserter(toExit, toExit.end())); // chunks we must be EXITed from (old - new)
    std::set_difference(newViewables.begin(), newViewables.end(), oldViewables.begin(), oldViewables.end(),
        std::inserter(toEnter, toEnter.end())); // chunks we must be ENTERed into (new - old)

    // update views
    removeEntityFromChunks(toExit, ref);
    addEntityToChunks(toEnter, ref);

    ent->chunkPos = to; // update cached chunk position
    // updated cached viewable chunks
    ent->viewableChunks.clear();
    ent->viewableChunks.insert(newViewables.begin(), newViewables.end());
}

bool Chunking::chunkExists(ChunkPos chunk) {
    return chunks.find(chunk) != chunks.end();
}

ChunkPos Chunking::chunkPosAt(int posX, int posY, uint64_t instanceID) {
    return ChunkPos(posX / (settings::VIEWDISTANCE / 3), posY / (settings::VIEWDISTANCE / 3), instanceID);
}

std::set<Chunk*> Chunking::getViewableChunks(ChunkPos chunk) {
    std::set<Chunk*> chnks;

    int x, y;
    uint64_t inst;
    std::tie(x, y, inst) = chunk;

    // grabs surrounding chunks if they exist
    for (int i = -1; i < 2; i++) {
        for (int z = -1; z < 2; z++) {
            ChunkPos pos = ChunkPos(x+i, y+z, inst);

            // if chunk exists, add it to the set
            if (chunkExists(pos))
                chnks.insert(chunks[pos]);
        }
    }

    return chnks;
}

/*
 * inefficient algorithm to get all chunks from a specific instance
 */
std::vector<ChunkPos> Chunking::getChunksInMap(uint64_t mapNum) {
    std::vector<ChunkPos> chnks;

    for (auto it = chunks.begin(); it != chunks.end(); it++) {
        if (std::get<2>(it->first) == mapNum) {
            chnks.push_back(it->first);
        }
    }

    return chnks;
}

/*
 * Used only for eggs; use npc->playersInView for everything visible
 */
bool Chunking::inPopulatedChunks(std::set<Chunk*>* chnks) {
    for (auto it = chnks->begin(); it != chnks->end(); it++) {
        if ((*it)->nplayers > 0)
            return true;
    }

    return false;
}

void Chunking::createInstance(uint64_t instanceID) {
    std::vector<ChunkPos> templateChunks = getChunksInMap(MAPNUM(instanceID)); // base instance chunks

    // only instantiate if the instance doesn't exist already
    if (getChunksInMap(instanceID).size() != 0) {
        std::cout << "Instance " << instanceID << " already exists" << std::endl;
        return;
    }

    std::cout << "Creating instance " << instanceID << std::endl;
    for (ChunkPos &coords : templateChunks) {
        for (const EntityRef ref : chunks[coords]->entities) {
            if (ref.kind == EntityKind::PLAYER)
                continue;

            int npcID = ref.id;
            BaseNPC* baseNPC = (BaseNPC*)ref.getEntity();

            // make a copy of each NPC in the template chunks and put them in the new instance
            if (baseNPC->kind == EntityKind::MOB) {
                if (((Mob*)baseNPC)->groupLeader != 0 && ((Mob*)baseNPC)->groupLeader != npcID)
                    continue; // follower; don't copy individually

                Mob* newMob = new Mob(baseNPC->x, baseNPC->y, baseNPC->z, baseNPC->angle,
                    instanceID, baseNPC->type, NPCManager::NPCData[baseNPC->type], NPCManager::nextId--);
                NPCManager::NPCs[newMob->id] = newMob;

                // if in a group, copy over group members as well
                if (((Mob*)baseNPC)->groupLeader != 0) {
                    newMob->groupLeader = newMob->id; // set leader ID for new leader
                    Mob* mobData = (Mob*)baseNPC;
                    for (int i = 0; i < 4; i++) {
                        if (mobData->groupMember[i] != 0) {
                            int followerID = NPCManager::nextId--; // id for follower
                            BaseNPC* baseFollower = NPCManager::NPCs[mobData->groupMember[i]]; // follower from template
                            // new follower instance
                            Mob* newMobFollower = new Mob(baseFollower->x, baseFollower->y, baseFollower->z, baseFollower->angle,
                                instanceID, baseFollower->type, NPCManager::NPCData[baseFollower->type], followerID);
                            // add follower to NPC maps
                            NPCManager::NPCs[followerID] = newMobFollower;
                            // set follower-specific properties
                            newMobFollower->groupLeader = newMob->id;
                            newMobFollower->offsetX = ((Mob*)baseFollower)->offsetX;
                            newMobFollower->offsetY = ((Mob*)baseFollower)->offsetY;
                            // add follower copy to leader copy
                            newMob->groupMember[i] = followerID;
                            NPCManager::updateNPCPosition(followerID, baseFollower->x, baseFollower->y, baseFollower->z,
                                instanceID, baseFollower->angle);
                        }
                    }
                }
                NPCManager::updateNPCPosition(newMob->id, baseNPC->x, baseNPC->y, baseNPC->z,
                    instanceID, baseNPC->angle);
            } else {
                BaseNPC* newNPC = new BaseNPC(baseNPC->angle, instanceID, baseNPC->type, NPCManager::nextId--);
                NPCManager::NPCs[newNPC->id] = newNPC;
                NPCManager::updateNPCPosition(newNPC->id, baseNPC->x, baseNPC->y, baseNPC->z,
                    instanceID, baseNPC->angle);
            }
        }
    }
}

static void destroyInstance(uint64_t instanceID) {
    std::vector<ChunkPos> instanceChunks = getChunksInMap(instanceID);
    std::cout << "Deleting instance " << instanceID << " (" << instanceChunks.size() << " chunks)" << std::endl;
    for (ChunkPos& coords : instanceChunks) {
        emptyChunk(coords);
    }
}

void Chunking::destroyInstanceIfEmpty(uint64_t instanceID) {
    if (PLAYERID(instanceID) == 0)
        return; // don't clean up overworld/IZ chunks

    std::vector<ChunkPos> sourceChunkCoords = getChunksInMap(instanceID);

    for (ChunkPos& coords : sourceChunkCoords) {
        Chunk* chunk = chunks[coords];

        if (chunk->nplayers > 0)
            return; // there are still players inside
    }

    destroyInstance(instanceID);
}
