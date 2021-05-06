#include "Chunking.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"
#include "settings.hpp"
#include "Combat.hpp"
#include "Eggs.hpp"

using namespace Chunking;

std::map<ChunkPos, Chunk*> Chunking::chunks;

static void newChunk(ChunkPos pos) {
    if (chunkExists(pos)) {
        std::cout << "[WARN] Tried to create a chunk that already exists\n";
        return;
    }

    Chunk *chunk = new Chunk();
    chunks[pos] = chunk;

    // add the chunk to the cache of all players and NPCs in the surrounding chunks
    std::set<Chunk*> surroundings = getViewableChunks(pos);
    for (Chunk* c : surroundings)
        for (const EntityRef& ref : c->entities)
            ref.getEntity()->viewableChunks.insert(chunk);
}

static void deleteChunk(ChunkPos pos) {
    if (!chunkExists(pos)) {
        std::cout << "[WARN] Tried to delete a chunk that doesn't exist\n";
        return;
    }

    Chunk* chunk = chunks[pos];

    // remove the chunk from the cache of all players and NPCs in the surrounding chunks
    std::set<Chunk*> surroundings = getViewableChunks(pos);
    for(Chunk* c : surroundings)
        for (const EntityRef& ref : c->entities)
            ref.getEntity()->viewableChunks.erase(chunk);

    chunks.erase(pos); // remove from map
    delete chunk; // free from memory
}

void Chunking::trackEntity(ChunkPos chunkPos, const EntityRef& ref) {
    if (!chunkExists(chunkPos))
        return; // shouldn't happen

    chunks[chunkPos]->entities.insert(ref);

    if (ref.type == EntityType::PLAYER)
        chunks[chunkPos]->nplayers++;
}

void Chunking::untrackEntity(ChunkPos chunkPos, const EntityRef& ref) {
    if (!chunkExists(chunkPos))
        return; // do nothing if chunk doesn't even exist

    Chunk* chunk = chunks[chunkPos];

    chunk->entities.erase(ref); // gone

    if (ref.type == EntityType::PLAYER)
        chunks[chunkPos]->nplayers--;
    assert(chunks[chunkPos]->nplayers >= 0);

    // if chunk is completely empty, free it
    if (chunk->entities.size() == 0)
        deleteChunk(chunkPos);
}

void Chunking::addEntityToChunks(std::set<Chunk*> chnks, const EntityRef& ref) {
    Entity *ent = ref.getEntity();
    bool alive = ent->isAlive();

    // TODO: maybe optimize this, potentially using AROUND packets?
    for (Chunk *chunk : chnks) {
        for (const EntityRef& otherRef : chunk->entities) {
            // skip oneself
            if (ref == otherRef)
                continue;

            Entity *other = otherRef.getEntity();

            // notify all visible players of the existence of this Entity
            if (alive && otherRef.type == EntityType::PLAYER) {
                ent->enterIntoViewOf(otherRef.sock);
            }

            // notify this *player* of the existence of all visible Entities
            if (ref.type == EntityType::PLAYER && other->isAlive()) {
                other->enterIntoViewOf(ref.sock);
            }

            // for mobs, increment playersInView
            if (ref.type == EntityType::MOB && otherRef.type == EntityType::PLAYER)
                ((Mob*)ent)->playersInView++;
            if (otherRef.type == EntityType::MOB && ref.type == EntityType::PLAYER)
                ((Mob*)other)->playersInView++;
        }
    }
}

void Chunking::removeEntityFromChunks(std::set<Chunk*> chnks, const EntityRef& ref) {
    Entity *ent = ref.getEntity();
    bool alive = ent->isAlive();

    // TODO: same as above
    for (Chunk *chunk : chnks) {
        for (const EntityRef& otherRef : chunk->entities) {
            // skip oneself
            if (ref == otherRef)
                continue;

            Entity *other = otherRef.getEntity();

            // notify all visible players of the departure of this Entity
            if (alive && otherRef.type == EntityType::PLAYER) {
                ent->disappearFromViewOf(otherRef.sock);
            }

            // notify this *player* of the departure of all visible Entities
            if (ref.type == EntityType::PLAYER && other->isAlive()) {
                other->disappearFromViewOf(ref.sock);
            }

            // for mobs, decrement playersInView
            if (ref.type == EntityType::MOB && otherRef.type == EntityType::PLAYER)
                ((Mob*)ent)->playersInView--;
            if (otherRef.type == EntityType::MOB && ref.type == EntityType::PLAYER)
                ((Mob*)other)->playersInView--;
        }
    }
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
    for (const EntityRef& ref : refs) {
        if (ref.type == EntityType::PLAYER)
            assert(0);

        // every call of this will check if the chunk is empty and delete it if so
        NPCManager::destroyNPC(ref.id);
    }
}

void Chunking::updateEntityChunk(const EntityRef& ref, ChunkPos from, ChunkPos to) {
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
    return std::make_tuple(posX / (settings::VIEWDISTANCE / 3), posY / (settings::VIEWDISTANCE / 3), instanceID);
}

std::set<Chunk*> Chunking::getViewableChunks(ChunkPos chunk) {
    std::set<Chunk*> chnks;

    int x, y;
    uint64_t inst;
    std::tie(x, y, inst) = chunk;

    // grabs surrounding chunks if they exist
    for (int i = -1; i < 2; i++) {
        for (int z = -1; z < 2; z++) {
            ChunkPos pos = std::make_tuple(x+i, y+z, inst);

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
        for (const EntityRef& ref : chunks[coords]->entities) {
            if (ref.type == EntityType::PLAYER)
                continue;

            int npcID = ref.id;
            BaseNPC* baseNPC = (BaseNPC*)ref.getEntity();

            // make a copy of each NPC in the template chunks and put them in the new instance
            if (baseNPC->type == EntityType::MOB) {
                if (((Mob*)baseNPC)->groupLeader != 0 && ((Mob*)baseNPC)->groupLeader != npcID)
                    continue; // follower; don't copy individually

                Mob* newMob = new Mob(baseNPC->x, baseNPC->y, baseNPC->z, baseNPC->appearanceData.iAngle,
                    instanceID, baseNPC->appearanceData.iNPCType, NPCManager::NPCData[baseNPC->appearanceData.iNPCType], NPCManager::nextId--);
                NPCManager::NPCs[newMob->appearanceData.iNPC_ID] = newMob;

                // if in a group, copy over group members as well
                if (((Mob*)baseNPC)->groupLeader != 0) {
                    newMob->groupLeader = newMob->appearanceData.iNPC_ID; // set leader ID for new leader
                    Mob* mobData = (Mob*)baseNPC;
                    for (int i = 0; i < 4; i++) {
                        if (mobData->groupMember[i] != 0) {
                            int followerID = NPCManager::nextId--; // id for follower
                            BaseNPC* baseFollower = NPCManager::NPCs[mobData->groupMember[i]]; // follower from template
                            // new follower instance
                            Mob* newMobFollower = new Mob(baseFollower->x, baseFollower->y, baseFollower->z, baseFollower->appearanceData.iAngle,
                                instanceID, baseFollower->appearanceData.iNPCType, NPCManager::NPCData[baseFollower->appearanceData.iNPCType], followerID);
                            // add follower to NPC maps
                            NPCManager::NPCs[followerID] = newMobFollower;
                            // set follower-specific properties
                            newMobFollower->groupLeader = newMob->appearanceData.iNPC_ID;
                            newMobFollower->offsetX = ((Mob*)baseFollower)->offsetX;
                            newMobFollower->offsetY = ((Mob*)baseFollower)->offsetY;
                            // add follower copy to leader copy
                            newMob->groupMember[i] = followerID;
                            NPCManager::updateNPCPosition(followerID, baseFollower->x, baseFollower->y, baseFollower->z,
                                instanceID, baseFollower->appearanceData.iAngle);
                        }
                    }
                }
                NPCManager::updateNPCPosition(newMob->appearanceData.iNPC_ID, baseNPC->x, baseNPC->y, baseNPC->z,
                    instanceID, baseNPC->appearanceData.iAngle);
            } else {
                BaseNPC* newNPC = new BaseNPC(baseNPC->x, baseNPC->y, baseNPC->z, baseNPC->appearanceData.iAngle,
                    instanceID, baseNPC->appearanceData.iNPCType, NPCManager::nextId--);
                NPCManager::NPCs[newNPC->appearanceData.iNPC_ID] = newNPC;
                NPCManager::updateNPCPosition(newNPC->appearanceData.iNPC_ID, baseNPC->x, baseNPC->y, baseNPC->z,
                    instanceID, baseNPC->appearanceData.iAngle);
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
