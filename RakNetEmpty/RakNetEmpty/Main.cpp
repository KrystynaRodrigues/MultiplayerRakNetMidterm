
#include "MessageIdentifiers.h"
#include "RakPeerInterface.h"
#include "BitStream.h"
#include <iostream>
#include <string>
#include <string.h>
#include <thread>         // std::thread
#include <chrono>
#include <map>
#include <algorithm>
#include <cstring>


static int SERVER_PORT = 60000;
static int CLIENT_PORT = 65001;
static int MAX_CONNECTIONS = 3;

RakNet::RakPeerInterface *g_rakPeerInterface = nullptr;

bool isServer = false;
bool isRunning = true;
bool playerSigningIn = false;
bool playerIsReady = false;
bool playerSelectedClass = false;
bool currentPlayersTurn = false;
bool playerIsDead = false;

unsigned short g_totalPlayers = 0;

int playersReady = 0;
int selectedClass = 0;
int playerIndex = 0;
int currentAlive = 0;

enum {
	LOBBY_ID = ID_USER_PACKET_ENUM,
	PLAYERS_READY_ID,
	CLASS_SELECTED_ID,
	PLAYER_ACTION_ID,
	SELECT_CHANGE_ID,
	PLAY_CHANGE_ID,
	CLASS_STATS_ID,
	PLAYER_TARGETS_ID,
	TURN_CHANGE_ID,
	PLAYER_HEAL_ID,
	PLAYER_ATTACK_ID,
	PRINT_COMMAND_ID,
	TRUE_ID,
	FALSE_ID,
	PLAYER_DEATH_ID,
	END_GAME_ID
};

class m_class
{
public:
	std::string className;
	int totalHealth;
	int weaponStrength;

	m_class(int health, int strength, std::string name) 
	{
		className = name;
		totalHealth = health;
		weaponStrength = strength;
	}

	void takeDamage(int weaponDamage) 
	{
		totalHealth -= weaponDamage;
	}

	int attackPlayer() 
	{
		int weaponDamage = rand() % (weaponStrength / 2);

		return weaponDamage;
	}

	int healSpell() 
	{
		int healedAmount = rand() % (totalHealth / 2);
		totalHealth += healedAmount;
		return healedAmount;
	}

	bool isDead() 
	{
		if (totalHealth <= 0) 
		{
			return true;
		}
		else 
		{
			return false;
		}
	}

	void classChosen() 
	{
		std::cout << "You have selected: " << std::endl;
		std::cout << className << "- \n Weapon Strength: " << weaponStrength << "\n Total Health: " << totalHealth << std::endl;
	}
};

m_class ranger = m_class(100, 50, "Ranger");
m_class paladin = m_class(100, 50, "Paladin");
m_class sorcerer = m_class(100, 50, "Sorcerer");

struct serverPlayer
{
	std::string name;
	RakNet::SystemAddress systemAddress;
	int playerIndex;
	m_class playerClass = m_class(0, 0, "");
	bool playerIsAlive = true;
};

RakNet::SystemAddress serverAddress;

std::map<unsigned long, serverPlayer> playerMap;


enum NetworkStates
{
	NS_Decision = 0,
	NS_CreateSocket,
	NS_PendingConnection,
	NS_Connected,
	NS_Running,
	NS_Lobby,
	NS_selectPlayer,
	NS_InGame,
	NS_playerDead,
	NS_EndGame
};

void packetsSentToClients(RakNet::MessageID id, std::string text)
{

	std::string textIn = text;
	RakNet::BitStream thisBitStream;
	thisBitStream.Write(id);
	RakNet::RakString name(textIn.c_str());
	thisBitStream.Write(name);

	for (auto const& x : playerMap)
	{
		g_rakPeerInterface->Send(&thisBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, x.second.systemAddress, false);
	}

}

NetworkStates g_networkState = NS_Decision;

void OnIncomingConnection(RakNet::Packet* packet)
{
	if (!isServer)
	{
		assert(0);
	}
	g_totalPlayers++;

	playerMap.insert(std::make_pair(RakNet::RakNetGUID::ToUint32(packet->guid), serverPlayer()));

	unsigned short numConnections = g_rakPeerInterface->NumberOfConnections();
	std::cout << "Total Players: " << playerMap.size()  << std::endl;
	std::cout << "Number of Connections: " << numConnections << std::endl;
}

void OnConnectionAccepted(RakNet::Packet* packet)
{
	if (isServer)
	{
		assert(0);
	}
	g_networkState = NS_Lobby;
	serverAddress = packet->systemAddress;
}

void InputHandler()
{
	while (isRunning)
	{
		char userInput[255];
		if (g_networkState == NS_Decision)
		{
			std::cout << "Press (s) for server, (c) for client" << std::endl;
			std::cin >> userInput;
			isServer = userInput[0] == 's';
			g_networkState = NS_CreateSocket;
		}
		else if (g_networkState == NS_CreateSocket)
		{
			if (isServer)
			{
				std::cout << "Server creating socket..." << std::endl;
			}
			else
			{
				std::cout << "Client creating socket..." << std::endl;
			}
		}
		else if (g_networkState == NS_Lobby)
		{
			if (playerSigningIn == false)
			{
				std::cout << "Enter your name to join the game!" << std::endl;
				std::cout << "To quit the game, type 'quit' " << std::endl;
				std::cin >> userInput;
				if (strcmp(userInput, "quit") == 0)
				{
					assert(0);
				}
				else
				{
					RakNet::BitStream thisBitStream;
					thisBitStream.Write((RakNet::MessageID)LOBBY_ID);
					RakNet::RakString name(userInput);
					thisBitStream.Write(name);
					g_rakPeerInterface->Send(&thisBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, serverAddress, false);

					playerSigningIn = true;

				}
			}
			else if (playerSigningIn == true && playerIsReady == false)
			{
				std::cout << "When you're ready to start, type 'ready'" << std::endl;
				std::cout << "To quit, type 'quit' " << std::endl;
				std::cin >> userInput;
				if (strcmp(userInput, "quit") == 0)
				{
					assert(0);
				}
				else if (strcmp(userInput, "ready") == 0)
				{
					RakNet::BitStream thisBitStream;
					thisBitStream.Write((RakNet::MessageID)PLAYERS_READY_ID);
					RakNet::RakString name(userInput);
					thisBitStream.Write(name);
					g_rakPeerInterface->Send(&thisBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, serverAddress, false);

					playerIsReady = true;
					std::cout << "Get ready! Waiting for other players. . ." << std::endl;
				}
			}
		}
		else if (g_networkState == NS_selectPlayer)
		{
			if (playerSelectedClass == false) {
				std::cout << "Select your class!" << std::endl;
				std::cout << "Type the name of the class you wish to play as." << std::endl;
				std::cout << "Ranger - Strength: 50, Health: 100" << std::endl;
				std::cout << "Paladin -  Strength 50, Health 100" << std::endl;
				std::cout << "Sorcerer -  Strength 50, Health 100" << std::endl;
				std::cin >> userInput;

				if (strcmp(userInput, "Ranger") == 0 || strcmp(userInput, "Paladin") == 0 || strcmp(userInput, "Sorcerer") == 0)
				{
					RakNet::BitStream thisBitStream;
					thisBitStream.Write((RakNet::MessageID)CLASS_SELECTED_ID);
					RakNet::RakString name(userInput);
					thisBitStream.Write(name);
					g_rakPeerInterface->Send(&thisBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, serverAddress, false);

					playerSelectedClass = true;
					std::cout << "Waiting for the other players to select a class. . ." << std::endl;
				}
			}
		}
		else if (g_networkState == NS_InGame)
		{
			if (playerIsDead == false) {
				if (currentPlayersTurn == true) {
					std::cout << "---------- YOUR TURN ----------" << std::endl;
					std::cout << "IT'S YOUR TURN!\n" << std::endl;
					std::cout << "You can attack another player or heal yourself." << std::endl;
					std::cout << "To attack a player, type that player's name" << std::endl;
					std::cout << "Type 'Heal' to heal yourself" << std::endl;
					std::cout << "To review your player stats, type 'Stats'!\n" << std::endl;


					RakNet::BitStream thisBitStream;
					thisBitStream.Write((RakNet::MessageID)PLAYER_TARGETS_ID);
					RakNet::RakString name(userInput);
					thisBitStream.Write(name);
					g_rakPeerInterface->Send(&thisBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, serverAddress, false);
					std::cin >> userInput;

					if (strcmp(userInput, "Stats") == 0)
					{
						RakNet::BitStream thisBitStream;
						thisBitStream.Write((RakNet::MessageID)CLASS_STATS_ID);
						RakNet::RakString name(userInput);
						thisBitStream.Write(name);
						g_rakPeerInterface->Send(&thisBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, serverAddress, false);
					}
					else if (strcmp(userInput, "Heal") == 0)
					{
						RakNet::BitStream thisBitStream;
						thisBitStream.Write((RakNet::MessageID)PLAYER_HEAL_ID);
						RakNet::RakString name(userInput);
						thisBitStream.Write(name);
						g_rakPeerInterface->Send(&thisBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, serverAddress, false);
						currentPlayersTurn = false;

						RakNet::BitStream thatBitStream;
						thatBitStream.Write((RakNet::MessageID)TURN_CHANGE_ID);
						RakNet::RakString name2(userInput);
						thatBitStream.Write(name2);
						g_rakPeerInterface->Send(&thatBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, serverAddress, false);
					}
					else
					{
						RakNet::BitStream thisBitStream;
						thisBitStream.Write((RakNet::MessageID)PLAYER_ATTACK_ID);
						RakNet::RakString name(userInput);
						thisBitStream.Write(name);
						g_rakPeerInterface->Send(&thisBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, serverAddress, false);
						currentPlayersTurn = false;

						RakNet::BitStream thatBitStream;
						thatBitStream.Write((RakNet::MessageID)TURN_CHANGE_ID);
						RakNet::RakString name2(userInput);
						thatBitStream.Write(name2);
						g_rakPeerInterface->Send(&thatBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, serverAddress, false);
					}
				}
			}
		}
		else if (g_networkState == NS_InGame)
		{
			if (playerIsDead == true)
			{
				std::cout << "You have been defeated and lost the game!" << std::endl;
			}
			else if (playerIsDead == false)
			{
				std::cout << "Congrats! You won the game!" << std::endl;
			}
		}

		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

unsigned char GetPacketIdentifier(RakNet::Packet *packet)
{
	if (packet == nullptr)
		return 255;

	if ((unsigned char)packet->data[0] == ID_TIMESTAMP)
	{
		RakAssert(packet->length > sizeof(RakNet::MessageID) + sizeof(RakNet::Time));
		return (unsigned char)packet->data[sizeof(RakNet::MessageID) + sizeof(RakNet::Time)];
	}
	else
		return (unsigned char)packet->data[0];
}

bool HandleLowLevelPacket(RakNet::Packet* packet)
{
	bool isHandled = true;
	unsigned char packetIdentifier = GetPacketIdentifier(packet);
	switch (packetIdentifier)
	{
	case ID_DISCONNECTION_NOTIFICATION:
		// Connection lost normally
		printf("ID_DISCONNECTION_NOTIFICATION\n");
		break;
	case ID_ALREADY_CONNECTED:
		// Connection lost normally
		printf("ID_ALREADY_CONNECTED with guid %" PRINTF_64_BIT_MODIFIER "u\n", packet->guid);
		break;
	case ID_INCOMPATIBLE_PROTOCOL_VERSION:
		printf("ID_INCOMPATIBLE_PROTOCOL_VERSION\n");
		break;
	case ID_REMOTE_DISCONNECTION_NOTIFICATION: // Server telling the clients of another client disconnecting gracefully.  You can manually broadcast this in a peer to peer enviroment if you want.
		printf("ID_REMOTE_DISCONNECTION_NOTIFICATION\n");
		break;
	case ID_REMOTE_CONNECTION_LOST: // Server telling the clients of another client disconnecting forcefully.  You can manually broadcast this in a peer to peer enviroment if you want.
		printf("ID_REMOTE_CONNECTION_LOST\n");
		break;
	case ID_NEW_INCOMING_CONNECTION:
	case ID_REMOTE_NEW_INCOMING_CONNECTION: // Server telling the clients of another client connecting.  You can manually broadcast this in a peer to peer enviroment if you want.
		printf("ID_REMOTE_NEW_INCOMING_CONNECTION\n");
		OnIncomingConnection(packet);
		break;
	case ID_CONNECTION_BANNED: // Banned from this server
		printf("We are banned from this server.\n");
		break;
	case ID_CONNECTION_ATTEMPT_FAILED:
		printf("Connection attempt failed\n");
		break;
	case ID_NO_FREE_INCOMING_CONNECTIONS:
		// Sorry, the server is full.  I don't do anything here but
		// A real app should tell the user
		printf("ID_NO_FREE_INCOMING_CONNECTIONS(Server Full)\n");
		break;
	case ID_INVALID_PASSWORD:
		printf("ID_INVALID_PASSWORD\n");
		break;
	case ID_CONNECTION_LOST:
		// Couldn't deliver a reliable packet - i.e. the other system was abnormally
		// terminated
		printf("ID_CONNECTION_LOST\n");
		break;
	case ID_CONNECTION_REQUEST_ACCEPTED:
		// This tells the client they have connected
		printf("ID_CONNECTION_REQUEST_ACCEPTED to %s with GUID %s\n", packet->systemAddress.ToString(true), packet->guid.ToString());
		printf("My external address is %s\n", g_rakPeerInterface->GetExternalID(packet->systemAddress).ToString(true));
		OnConnectionAccepted(packet);
		break;
	case ID_CONNECTED_PING:
	case ID_UNCONNECTED_PING:
		printf("Ping from %s\n", packet->systemAddress.ToString(true));
		break;

	case SELECT_CHANGE_ID:
		g_networkState = NS_selectPlayer;
		break;

	case PLAY_CHANGE_ID:
		g_networkState = NS_InGame;
		break;

	case PRINT_COMMAND_ID:
	{
		RakNet::BitStream thisBitStream(packet->data, packet->length, false); 
		RakNet::MessageID messageID;
		thisBitStream.Read(messageID);
		RakNet::RakString input;
		thisBitStream.Read(input);

		std::cout << input << std::endl;

		break;
	}

	case TRUE_ID:
	{
		currentPlayersTurn = true;
		break;
	}

	case FALSE_ID:
	{
		currentPlayersTurn = false;
		break;
	}

	case PLAYER_DEATH_ID:
	{
		playerIsDead = true;
		break;
	}

	case END_GAME_ID:
	{
		std::cout << "---------- Game Over ----------\n" << std::endl;

		if (playerIsDead == true)
		{
			std::cout << "You have been defeated and lost the game!" << std::endl;
		}
		else if (playerIsDead == false)
		{
			std::cout << "Congrats! You Won the game!" << std::endl;
		}

		g_networkState = NS_EndGame;
		break;
	}

	default:
		isHandled = false;
		break;
	}
	return isHandled;
}

void PacketHandler()
{
	while (isRunning)
	{
		for (RakNet::Packet* packet = g_rakPeerInterface->Receive(); packet != nullptr; g_rakPeerInterface->DeallocatePacket(packet), packet = g_rakPeerInterface->Receive())
		{
			

			if (!HandleLowLevelPacket(packet))
			{
				unsigned char packetIdentifier = GetPacketIdentifier(packet);
				switch (packetIdentifier)
				{
				case LOBBY_ID:
				{
					RakNet::BitStream thisBitStream(packet->data, packet->length, false); 
					RakNet::MessageID messageID;
					thisBitStream.Read(messageID);
					RakNet::RakString userName;
					thisBitStream.Read(userName);

					//storing player guid, name, and adress
					unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
					playerMap.find(guid)->second.name = userName;
					playerMap.find(guid)->second.systemAddress = packet->systemAddress;
					playerMap.find(guid)->second.playerIndex = g_totalPlayers;


					std::cout << "Welcome" << userName << "! " << userName << " has joined the game! " << std::endl;
					break;
				}


				case PLAYERS_READY_ID:
				{
					RakNet::BitStream thisBitStream(packet->data, packet->length, false);
					RakNet::MessageID messageID;
					thisBitStream.Read(messageID);
					RakNet::RakString userName;
					thisBitStream.Read(userName);

					unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
					std::string playerName = playerMap.find(guid)->second.name;
					playerMap.find(guid)->second.playerIndex = playersReady;

					std::cout << playerName.c_str() << " is ready! " << std::endl;

					playersReady++;

					if (playersReady >= g_totalPlayers) {
						currentAlive = g_totalPlayers;
						std::cout << "Players are selecting a class!" << std::endl;
						packetsSentToClients(SELECT_CHANGE_ID, "null");
					}
					break;
				}

				case CLASS_SELECTED_ID:
				{
					RakNet::BitStream thisBitStream(packet->data, packet->length, false);
					RakNet::MessageID messageID;
					thisBitStream.Read(messageID);
					RakNet::RakString className;
					thisBitStream.Read(className);

					unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);

					if (className == "Ranger")
					{
						playerMap.find(guid)->second.playerClass = ranger;
					}

					if (className == "Paladin")
					{
						playerMap.find(guid)->second.playerClass = paladin;
					}

					if (className == "Sorcerer")
					{
						playerMap.find(guid)->second.playerClass = sorcerer;
					}

					selectedClass++;

					if (selectedClass >= g_totalPlayers)
					{
						unsigned long guidTemp;
						for (auto const& x : playerMap)
						{
							if (x.second.playerIndex == playerIndex)
							{
								guidTemp = x.first;
							}
						}
						std::string nullTxt;
						RakNet::BitStream setBitStreamTurn;
						setBitStreamTurn.Write((RakNet::MessageID)TRUE_ID);
						RakNet::RakString writerturn(nullTxt.c_str());
						setBitStreamTurn.Write(writerturn);

						g_rakPeerInterface->Send(&setBitStreamTurn, HIGH_PRIORITY, RELIABLE_ORDERED, 0, playerMap.find(guidTemp)->second.systemAddress, false);

						std::string input = "The game has started!\n";

						RakNet::BitStream thisBitStreamOut;
						thisBitStreamOut.Write((RakNet::MessageID)PRINT_COMMAND_ID);
						RakNet::RakString writer(input.c_str());
						thisBitStreamOut.Write(writer);

						g_rakPeerInterface->Send(&thisBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

			
						std::string input2 = "\n It's " + playerMap.find(guidTemp)->second.name + "'s turn!\n";

						RakNet::BitStream thatBitStreamOut;
						thatBitStreamOut.Write((RakNet::MessageID)PRINT_COMMAND_ID);
						RakNet::RakString writer2nd(input2.c_str());
						thatBitStreamOut.Write(writer2nd);

						g_rakPeerInterface->Send(&thatBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, playerMap.find(guidTemp)->second.systemAddress, true);
						std::cout << "Game Phase" << std::endl;

						packetsSentToClients(PLAY_CHANGE_ID, "null");
					}
					break;
				}

				case PLAYER_TARGETS_ID:
				{
					RakNet::BitStream thisBitStream(packet->data, packet->length, false); 
					RakNet::MessageID messageID;
					thisBitStream.Read(messageID);
					RakNet::RakString nullIn;
					thisBitStream.Read(nullIn);

					unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);

					std::string input = " \n Possible attack targets:";

					RakNet::BitStream thisBitStreamOut;
					thisBitStreamOut.Write((RakNet::MessageID)PRINT_COMMAND_ID);
					RakNet::RakString writer(input.c_str());
					thisBitStreamOut.Write(writer);

					g_rakPeerInterface->Send(&thisBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

					for (auto const& x : playerMap)
					{
						if (x.first != guid)
						{
							if (x.second.playerIsAlive == true)
							{
								std::string input = x.second.name;
								RakNet::BitStream thisBitStreamOut;
								thisBitStreamOut.Write((RakNet::MessageID)PRINT_COMMAND_ID);
								RakNet::RakString writer(input.c_str());
								thisBitStreamOut.Write(writer);

								g_rakPeerInterface->Send(&thisBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);
							}
						}
					}

					std::string input2 = " ";

					RakNet::BitStream thatBitStreamOut;
					thatBitStreamOut.Write((RakNet::MessageID)PRINT_COMMAND_ID);
					RakNet::RakString writer2nd(input2.c_str());
					thatBitStreamOut.Write(writer2nd);

					g_rakPeerInterface->Send(&thatBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

					break;
				}

				case CLASS_STATS_ID:
				{
					RakNet::BitStream thisBitStream(packet->data, packet->length, false); 
					RakNet::MessageID messageID;
					thisBitStream.Read(messageID);
					RakNet::RakString nullIn;
					thisBitStream.Read(nullIn);

					unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);

					std::string input = "Your Class: " + playerMap.find(guid)->second.playerClass.className;
					std::string input2 = "Total Health: " + std::to_string(playerMap.find(guid)->second.playerClass.totalHealth);
					std::string input3 = "Weapon Strength: " + std::to_string(playerMap.find(guid)->second.playerClass.weaponStrength) + "\n";

					
					RakNet::BitStream thisBitStreamOut;
					thisBitStreamOut.Write((RakNet::MessageID)PRINT_COMMAND_ID);
					RakNet::RakString writer(input.c_str());
					thisBitStreamOut.Write(writer);

					g_rakPeerInterface->Send(&thisBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

				
					RakNet::BitStream thatBitStreamOut;
					thatBitStreamOut.Write((RakNet::MessageID)PRINT_COMMAND_ID);
					RakNet::RakString writer2nd(input2.c_str());
					thatBitStreamOut.Write(writer2nd);

					g_rakPeerInterface->Send(&thatBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

			
					RakNet::BitStream BitStreamOut;
					BitStreamOut.Write((RakNet::MessageID)PRINT_COMMAND_ID);
					RakNet::RakString writer3rd(input3.c_str());
					BitStreamOut.Write(writer3rd);

					g_rakPeerInterface->Send(&BitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);


					break;
				}

				case TURN_CHANGE_ID:
				{
					if (currentAlive > 1) {
						unsigned long guidTemp;
						for (auto const& x : playerMap)
						{
							if (x.second.playerIndex == playerIndex)
							{
								if (x.second.playerIsAlive == false)
								{
									playerIndex++;
									if (playerIndex >= g_totalPlayers) {
										playerIndex = 0;
									}
								}
								else
								{
									guidTemp = x.first;
								}
							}
						}
						std::string nullTxt;
						RakNet::BitStream setBitStreamTurn;
						setBitStreamTurn.Write((RakNet::MessageID)TRUE_ID);
						RakNet::RakString writerturn(nullTxt.c_str());
						setBitStreamTurn.Write(writerturn);

						g_rakPeerInterface->Send(&setBitStreamTurn, HIGH_PRIORITY, RELIABLE_ORDERED, 0, playerMap.find(guidTemp)->second.systemAddress, false);

		
						std::string input2 = "\n It's " + playerMap.find(guidTemp)->second.name + "'s turn!\n";

						RakNet::BitStream thatBitStreamOut;
						thatBitStreamOut.Write((RakNet::MessageID)PRINT_COMMAND_ID);
						RakNet::RakString writer2nd(input2.c_str());
						thatBitStreamOut.Write(writer2nd);

						g_rakPeerInterface->Send(&thatBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, playerMap.find(guidTemp)->second.systemAddress, true);
					}
					break;
				}

				case PLAYER_HEAL_ID:
				{
					RakNet::BitStream thisBitStream(packet->data, packet->length, false); 
					RakNet::MessageID messageID;
					thisBitStream.Read(messageID);
					RakNet::RakString nullIn;
					thisBitStream.Read(nullIn);

					unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);

					int healAmount = playerMap.find(guid)->second.playerClass.healSpell();
					std::string input = "\n You use your healing spell and heal for: " + std::to_string(healAmount) + ".\n Your total health is: " + std::to_string(playerMap.find(guid)->second.playerClass.totalHealth) + " health.";

					RakNet::BitStream thisBitStreamOut;
					thisBitStreamOut.Write((RakNet::MessageID)PRINT_COMMAND_ID);
					RakNet::RakString writer(input.c_str());
					thisBitStreamOut.Write(writer);

					g_rakPeerInterface->Send(&thisBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, playerMap.find(guid)->second.systemAddress, false);

					std::string input2 = "    " + playerMap.find(guid)->second.name + " used a healing spell and healed for: " + std::to_string(healAmount) + ".\n They now have: " + std::to_string(playerMap.find(guid)->second.playerClass.totalHealth) + " health.";

					RakNet::BitStream thatBitStreamOut;
					thatBitStreamOut.Write((RakNet::MessageID)PRINT_COMMAND_ID);
					RakNet::RakString writer2nd(input2.c_str());
					thatBitStreamOut.Write(writer2nd);

					g_rakPeerInterface->Send(&thatBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, playerMap.find(guid)->second.systemAddress, true);

					playerIndex++;
					if (playerIndex >= g_totalPlayers) {
						playerIndex = 0;
					}
					break;
				}

				case PLAYER_ATTACK_ID:
				{
					RakNet::BitStream thisBitStream(packet->data, packet->length, false); 
					RakNet::MessageID messageID;
					thisBitStream.Read(messageID);
					RakNet::RakString name;
					thisBitStream.Read(name);

					unsigned long inputGuid = RakNet::RakNetGUID::ToUint32(packet->guid);

					unsigned long guidTemp;
					for (auto const& x : playerMap)
					{
						std::cout << x.second.name.c_str() << ", attacks: " << name << std::endl;

						if (strcmp(x.second.name.c_str(), name) == 0) {
							guidTemp = x.first;
						}
					}

					if (playerMap.find(guidTemp)->second.playerIsAlive == true)
					{

						int attackDamage = playerMap.find(inputGuid)->second.playerClass.attackPlayer();
						playerMap.find(guidTemp)->second.playerClass.takeDamage(attackDamage);

						std::string input = "\n You raise your weapon and attack. You deal " + std::to_string(attackDamage) + " damage to " + playerMap.find(guidTemp)->second.name +
							".\n They now have: " + std::to_string(playerMap.find(guidTemp)->second.playerClass.totalHealth) + " health.";

						RakNet::BitStream thisBitStreamOut;
						thisBitStreamOut.Write((RakNet::MessageID)PRINT_COMMAND_ID);
						RakNet::RakString writer(input.c_str());
						thisBitStreamOut.Write(writer);

						g_rakPeerInterface->Send(&thisBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, playerMap.find(inputGuid)->second.systemAddress, false);

						std::string input2 = "      " + playerMap.find(inputGuid)->second.name + " attacks and deals " + std::to_string(attackDamage) + " damage to " + playerMap.find(guidTemp)->second.name +
							".\n  " + playerMap.find(guidTemp)->second.name + " now has: " + std::to_string(playerMap.find(guidTemp)->second.playerClass.totalHealth) + " health.";

						RakNet::BitStream thatBitStreamOut;
						thatBitStreamOut.Write((RakNet::MessageID)PRINT_COMMAND_ID);
						RakNet::RakString writer2nd(input2.c_str());
						thatBitStreamOut.Write(writer2nd);

						g_rakPeerInterface->Send(&thatBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, playerMap.find(inputGuid)->second.systemAddress, true);


						if (playerMap.find(guidTemp)->second.playerClass.isDead())
						{
			
							RakNet::BitStream deathBitStream;
							deathBitStream.Write((RakNet::MessageID)PLAYER_DEATH_ID);
							RakNet::RakString writer(input2.c_str());
							deathBitStream.Write(writer);

							g_rakPeerInterface->Send(&deathBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, playerMap.find(guidTemp)->second.systemAddress, false);

							playerMap.find(guidTemp)->second.playerIsAlive = false;

							currentAlive--;

							std::string input = "      " + playerMap.find(guidTemp)->second.name + " has been defeated!";

							RakNet::BitStream thisBitStreamOut;
							thisBitStreamOut.Write((RakNet::MessageID)PRINT_COMMAND_ID);
							RakNet::RakString writer2nd(input.c_str());
							thisBitStreamOut.Write(writer2nd);

							g_rakPeerInterface->Send(&thisBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, playerMap.find(guidTemp)->second.systemAddress, true);

							std::string input2 = "      You have been defeated!";

							RakNet::BitStream thatBitStreamOut;
							thatBitStreamOut.Write((RakNet::MessageID)PRINT_COMMAND_ID);
							RakNet::RakString writer3rd(input2.c_str());
							thatBitStreamOut.Write(writer3rd);

							g_rakPeerInterface->Send(&thatBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, playerMap.find(guidTemp)->second.systemAddress, false);
						}
					}
					else 
					{
						std::string input2 = "      The player you selected has been already been defeated! Next turn.\n";

						RakNet::BitStream thisBitStreamOut;
						thisBitStreamOut.Write((RakNet::MessageID)PRINT_COMMAND_ID);
						RakNet::RakString writer2nd(input2.c_str());
						thisBitStreamOut.Write(writer2nd);

						g_rakPeerInterface->Send(&thisBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, playerMap.find(inputGuid)->second.systemAddress, true);
						g_rakPeerInterface->Send(&thisBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, playerMap.find(inputGuid)->second.systemAddress, false);
					}

					if (currentAlive > 1) 
					{

						playerIndex++;
						if (playerIndex >= g_totalPlayers) 
						{
							playerIndex = 0;
						}
					}
					else 
					{
						packetsSentToClients(END_GAME_ID, "null");
						std::cout << " Game End" << std::endl;
					}
					break;
				}

				default:
				
					printf("%s\n", packet->data);
					break;
				}
			}
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

int main()
{
	g_rakPeerInterface = RakNet::RakPeerInterface::GetInstance();

	std::thread inputHandler(InputHandler);
	std::thread packetHandler(PacketHandler);


	while (isRunning)
	{
		if (g_networkState == NS_CreateSocket)
		{
			if (isServer)
			{
				//opening up the server socket
				RakNet::SocketDescriptor socketDescriptors[1];
				socketDescriptors[0].port = SERVER_PORT;
				socketDescriptors[0].socketFamily = AF_INET; // Test out IPV4
				bool isSuccess = g_rakPeerInterface->Startup(MAX_CONNECTIONS, socketDescriptors, 1) == RakNet::RAKNET_STARTED;
				assert(isSuccess);
				g_rakPeerInterface->SetMaximumIncomingConnections(MAX_CONNECTIONS);
				g_networkState = NS_PendingConnection;
				std::cout << "Server waiting on connections.." << std::endl;
			}
			else
			{

				//creating a socket for communication
				RakNet::SocketDescriptor socketDescriptor(CLIENT_PORT, nullptr);
				socketDescriptor.socketFamily = AF_INET;

				while (RakNet::IRNS2_Berkley::IsPortInUse(socketDescriptor.port, socketDescriptor.hostAddress, socketDescriptor.socketFamily, SOCK_DGRAM) == true)
					socketDescriptor.port++;

				g_rakPeerInterface->Startup(8, &socketDescriptor, 1);

				//client connection
				//127.0.0.1 is localhost aka yourself
				RakNet::ConnectionAttemptResult car = g_rakPeerInterface->Connect("127.0.0.1", SERVER_PORT, nullptr, 0);
				RakAssert(car == RakNet::CONNECTION_ATTEMPT_STARTED);
				std::cout << "client attempted connection..waiting for response" << std::endl;
				g_networkState = NS_PendingConnection;
			}
		}

	}

	inputHandler.join();
	packetHandler.join();
	return 0;
}