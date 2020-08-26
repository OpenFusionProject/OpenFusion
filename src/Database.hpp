#pragma once

#include "CNStructs.hpp"
#include "Player.hpp"

#include <list>
#include <string>

namespace Database {
	void open();
	//checks regex
	bool isLoginDataGood(std::string login, std::string password);
	void addAccount(std::string login, std::string password);
	bool doesUserExist(std::string login);
	bool isPasswordCorrect(std::string login, std::string password);
	int getUserID(std::string login);
	int getUserSlotsNum(int AccountId);

	bool isNameFree(std::string First, std::string Second);
	//after chosing name
	void createCharacter(sP_CL2LS_REQ_SAVE_CHAR_NAME* save, int AccountID);
	//after finishing creation
	void finishCharacter(sP_CL2LS_REQ_CHAR_CREATE* character);
	void finishTutorial(int PlayerID);
	//returns slot nr if success
	int deleteCharacter(int characterID, int accountID);
	int getCharacterID(int AccountID, int slot);

	std::list <Player> getCharacters(int UserID);

	//some json parsing crap
	std::string CharacterToJson(sP_CL2LS_REQ_SAVE_CHAR_NAME* save);
	std::string CharacterToJson(sP_CL2LS_REQ_CHAR_CREATE* character);
	Player JsonToPlayer(std::string input, int PC_UID);
	std::string PlayerToJson(Player player);


}