#include "server/server.hpp"
#include "server/client_connection.hpp"

using asio::ip::tcp;

namespace tec {
	namespace networking {
		const int SERVER_PORT = 0xa10c;

		Server::Server(tcp::endpoint& endpoint) : acceptor(io_service, endpoint), socket(io_service) {
			std::string message("Hello from server\n");
			greeting_msg.body_length(message.size());
			memcpy(greeting_msg.body(), message.c_str(), greeting_msg.body_length());
			greeting_msg.encode_header();
			do_accept();
		}

		void Server::Deliver(const chat_message& msg) {
			this->recent_msgs.push_back(msg);
			while (this->recent_msgs.size() > max_recent_msgs) {
				this->recent_msgs.pop_front();
			}

			for (auto client : this->clients) {
				client->QueueWrite(msg);
			}
		}

		void Server::Leave(std::shared_ptr<ClientConnection> client) {
			this->clients.erase(client);
		}

		void Server::Start() {
			this->io_service.run();
		}

		void Server::Stop() {
			this->io_service.stop();
		}

		void Server::do_accept() {
			acceptor.async_accept(socket,
				[this] (std::error_code error) {
				if (!error) {
					asio::write(socket, asio::buffer(greeting_msg.data(), greeting_msg.length()));
					std::shared_ptr<ClientConnection> client = std::make_shared<ClientConnection>(std::move(socket), this);
					clients.insert(client);
					for (auto msg : this->recent_msgs) {
						client->QueueWrite(msg);
					}
					client->StartRead();
				}

				do_accept();
			});
		}
	}
}
