


#pragma once

#include <memory>
#include <set>
#include <map>
#include <glm/mat4x4.hpp>

#ifndef __APPLE__
#include <GL/glew.h>
#else
#include <OpenGL/gl3.h>
#endif

#include "types.hpp"
#include "game-state.hpp"
#include "event-system.hpp"
#include "command-queue.hpp"
#include "graphics/gbuffer.hpp"
#include "graphics/vertex-buffer-object.hpp"

namespace spdlog {
	class logger;
}

namespace tec {
	struct VertexGroup;
	struct View;
	class Shader;
	class Animation;

	class RenderSystem;
	typedef Command<RenderSystem> RenderCommand;

	struct WindowResizedEvent;
	struct EntityDestroyed;

	class RenderSystem : public CommandQueue<RenderSystem>,
		public EventQueue<WindowResizedEvent>,
		public EventQueue<EntityDestroyed>, public EventQueue<EntityCreated> {
	public:
		RenderSystem();

		void SetViewportSize(const unsigned int width, const unsigned int height);

		void Update(const double delta, const GameState& state);
	private:
		std::shared_ptr<spdlog::logger> _log;

		void GeometryPass();
		void BeginPointLightPass();
		void DirectionalLightPass();
		void FinalPass();
		void RenderGbuffer();

		void SetupDefaultShaders();

		using EventQueue<WindowResizedEvent>::On;
		using EventQueue<EntityDestroyed>::On;
		using EventQueue<EntityCreated>::On;
		void On(std::shared_ptr<WindowResizedEvent> data);
		void On(std::shared_ptr<EntityDestroyed> data);
		void On(std::shared_ptr<EntityCreated> data);
		void UpdateRenderList(double delta, const GameState& state);

		glm::mat4 projection{ 0 };
		View* current_view{ nullptr };
		unsigned int window_width{ 1024 }, window_height{ 768 };
		std::map<eid, glm::mat4> model_matricies;
		std::shared_ptr<Shader> default_shader;

		GBuffer light_gbuffer;
		VertexBufferObject sphere_vbo; // Used for rendering point lights.
		VertexBufferObject quad_vbo; // Used for rendering directional lights.

		struct RenderItem {
			glm::mat4* model_matrix{ nullptr };
			std::set<VertexGroup*>* vertex_groups{ nullptr };
			GLuint vao{ 0 }, ibo{ 0 };
			bool animated{ false };
			Animation* animation{ nullptr };

			friend bool operator<(const RenderItem& a, const RenderItem& b) {
				return a.vao < b.vao;
			}
		};
		std::map<std::shared_ptr<Shader>, std::set<RenderItem>> render_item_list;
	};
}
