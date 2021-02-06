


#include "server-connection.hpp"
#include "events.hpp"
#include "event-system.hpp"
#include "game-state.hpp"

using asio::ip::tcp;

namespace tec {
	namespace networking {

		const std::string_view SERVER_PORT = "41228";
		const std::string_view LOCAL_HOST = "127.0.0.1";

		std::shared_ptr<spdlog::logger> ServerConnection::_log;
		std::mutex ServerConnection::recent_ping_mutex;

		ServerConnection::ServerConnection() : socket(io_service) {
			_log = spdlog::get("console_log");
			RegisterMessageHandler(MessageType::SYNC, [this] (const ServerMessage& message) {
				this->SyncHandler(message);
								   });
			RegisterMessageHandler(MessageType::GAME_STATE_UPDATE, [this] (const ServerMessage& message) {
				this->GameStateUpdateHandler(message);
								   });
			RegisterMessageHandler(MessageType::CHAT_MESSAGE, [] (const ServerMessage& message) {
				std::string msg(message.GetBodyPTR(), message.GetBodyLength());
				_log->info(msg);
								   });
			RegisterMessageHandler(MessageType::CLIENT_ID, [this] (const ServerMessage& message) {
				std::string id_message(message.GetBodyPTR(), message.GetBodyLength());
				this->client_id = std::atoi(id_message.c_str());
								   });
			RegisterMessageHandler(MessageType::CLIENT_LEAVE, [] (const ServerMessage& message) {
				std::string id_message(message.GetBodyPTR(), message.GetBodyLength());
				eid entity_id = std::atoi(id_message.c_str());
				_log->info("Entity " + std::to_string(entity_id) + " left");
				std::shared_ptr<EntityDestroyed> data = std::make_shared<EntityDestroyed>();
				data->entity_id = entity_id;
				EventSystem<EntityDestroyed>::Get()->Emit(data);
								   });
			RegisterMessageHandler(MessageType::ENTITY_CREATE, [this](const ServerMessage&) {
				std::shared_ptr<EntityCreated> data = std::make_shared<EntityCreated>();
				data->entity.ParseFromArray(current_read_msg.GetBodyPTR(), static_cast<int>(current_read_msg.GetBodyLength()));
				data->entity_id = data->entity.id();
				EventSystem<EntityCreated>::Get()->Emit(data);
								   });
		}

		bool ServerConnection::Connect(std::string_view ip) {
			this->client_id = 0;
			this->socket.close();
			tcp::resolver resolver(this->io_service);
			tcp::resolver::query query(std::string(ip).data(), std::string(SERVER_PORT).data());
			asio::error_code error = asio::error::host_not_found;
			tcp::resolver::iterator endpoint_iterator = resolver.resolve(query), end;
			try {
				while (error && endpoint_iterator != end) {
					this->socket.close();
					this->socket.connect(*endpoint_iterator++, error);
				}
				if (error) {
					this->socket.close();
					throw asio::system_error(error);
				}
			}
			catch (std::exception & e) {
				std::cerr << e.what() << std::endl;
				return false;
			}

			if (this->onConnect) {
				this->onConnect();
			}

			return true;
		}

		void ServerConnection::Disconnect() {
			this->socket.close();
			this->stopped = true;
		}

		void ServerConnection::Stop() {
			this->stopped = true;
		}

		void ServerConnection::SendChatMessage(std::string message) {
			if (this->socket.is_open()) {
				ServerMessage msg;
				msg.SetBodyLength(message.size());
				memcpy(msg.GetBodyPTR(), message.c_str(), msg.GetBodyLength());
				msg.encode_header();
				Send(msg);
			}
		}

		void ServerConnection::Send(ServerMessage& msg) {
			try {
				asio::write(this->socket, asio::buffer(msg.GetDataPTR(), msg.length()));
			}
			catch (std::exception const& e) {
				std::cerr << "ServerConnection::Send(ServerMessage&): asio::write:" << e.what() << std::endl;
			}
		}

		void ServerConnection::RegisterConnectFunc(std::function<void()> func) {
			this->onConnect = std::move(func);
		}

		void ServerConnection::read_body() {
			asio::error_code error = asio::error::eof;
			asio::read(this->socket,
					   asio::buffer(current_read_msg.GetBodyPTR(), current_read_msg.GetBodyLength()),
					   error);

			if (!error) {
				for (auto handler : this->message_handlers[current_read_msg.GetMessageType()]) {
					handler(current_read_msg);
				}
			}
			else if (error) {
				this->socket.close();
				throw asio::system_error(error);
			}
		}

		void ServerConnection::read_header() {
			asio::error_code error = asio::error::eof;
			asio::read(this->socket,
					   asio::buffer(this->current_read_msg.GetDataPTR(), ServerMessage::header_length), error);
			this->recv_time = std::chrono::high_resolution_clock::now();

			if (!error && this->current_read_msg.decode_header()) {
				read_body();
			}
			else if (error) {
				this->socket.close();
				throw asio::system_error(error);
			}
		}

		void ServerConnection::StartRead() {
			this->stopped = false;
			while (!this->stopped) {
				try {
					if (this->socket.is_open() && this->socket.available()) {
						read_header();
					}
				}
				catch (std::exception const& e) {
					std::cerr << "ServerConnection::StartRead(): Error reading header:" << e.what() << std::endl;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}

		void ServerConnection::StartSync() {
			ServerMessage sync_msg;

			sync_msg.SetBodyLength(1);
			sync_msg.SetMessageType(MessageType::SYNC);
			sync_msg.encode_header();
			while (1) {
				if (this->stopped) {
					return;
				}
				Send(sync_msg);
				this->sync_start = std::chrono::high_resolution_clock::now();
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}

		void ServerConnection::SyncHandler(const ServerMessage&) {
			std::chrono::milliseconds round_trip = std::chrono::duration_cast<std::chrono::milliseconds>(recv_time - sync_start);
			std::lock_guard<std::mutex> recent_ping_lock(recent_ping_mutex);
			if (this->recent_pings.size() >= 10) {
				this->recent_pings.pop_front();
			}
			this->recent_pings.push_back(round_trip.count() / 2);
			ping_time_t total_pings = 0;
			for (ping_time_t ping : this->recent_pings) {
				total_pings += ping;
			}
			this->average_ping = total_pings / 10;
		}

		void ServerConnection::GameStateUpdateHandler(const ServerMessage& message) {
			proto::GameStateUpdate gsu;
			gsu.ParseFromArray(message.GetBodyPTR(), static_cast<int>(message.GetBodyLength()));
			state_id_t recv_state_id = gsu.state_id();
			if (recv_state_id <= this->last_received_state_id) {
				_log->warn("Received an older GameStateUpdate");
			}
			else {
				this->last_received_state_id = recv_state_id;
				GameState next_state;
				next_state.In(gsu);
				std::shared_ptr<NewGameStateEvent> new_game_state_msg = std::make_shared<NewGameStateEvent>();
				new_game_state_msg->new_state = std::move(next_state);
				EventSystem<NewGameStateEvent>::Get()->Emit(new_game_state_msg);
			}
			//_log->info(last_received_state_id);
		}
	}
}
