


#pragma once

#include <set>
#include <deque>
#include <mutex>

#include <asio.hpp>
#include <components.pb.h>

#include "server-message.hpp"
#include "event-queue.hpp"
#include "event-system.hpp"
#include "events.hpp"

using asio::ip::tcp;

namespace tec {
	namespace networking {

		extern unsigned short PORT;
		static eid BASE_ENTITY_ID = 10000;

		class ClientConnection;

		class Server : public EventQueue<EntityCreated>, public EventQueue<EntityDestroyed> {
		public:
			Server(tcp::endpoint& endpoint);

			// Deliver a message to all clients.
			// save_to_recent is used to save a recent list of message each client gets when they connect.
			void Deliver(const ServerMessage& msg, bool save_to_recent = true);

			// Deliver a message to a specific client.
			void Deliver(std::shared_ptr<ClientConnection> client, const ServerMessage& msg);

			// Calls when a client leaves, usually when the connection is no longer valid.
			void Leave(std::shared_ptr<ClientConnection> client);

			void Start();

			void Stop();

			// Get a list of all connected clients.
			const std::set<std::shared_ptr<ClientConnection>>& GetClients() {
				return this->clients;
			}

			using EventQueue<EntityCreated>::On;
			using EventQueue<EntityDestroyed>::On;
			void On(std::shared_ptr<EntityCreated> data);
			void On(std::shared_ptr<EntityDestroyed> data);
		private:
			// Method that handles and accepts incoming connections.
			void AcceptHandler();
			
			// ASIO variables
			asio::io_service io_service;
			tcp::acceptor acceptor;
			tcp::socket socket;

			ServerMessage greeting_msg; // Greeting chat message.

			std::map<eid, proto::Entity> entities;

			std::set<std::shared_ptr<ClientConnection>> clients; // All connected clients.
			std::uint64_t base_id = BASE_ENTITY_ID; // Starting client_id

			// Recent message list all clients get on connecting,
			enum { max_recent_msgs = 100 };
			std::deque<ServerMessage> recent_msgs;
			static std::mutex recent_msgs_mutex;
		public:
			std::mutex client_list_mutex;
		};
	}
}
