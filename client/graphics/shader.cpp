


#include "shader.hpp"

#include <iterator>

#include <spdlog/spdlog.h>

namespace tec {
	Shader::~Shader() {
		DeleteProgram();
	}

	void Shader::DeleteProgram() {
		if (this->program != 0) {
			glDeleteProgram(this->program);
		}
		this->program = 0;
		for (auto s : this->shaders) {
			glDeleteShader(s);
		}
		this->shaders.clear();
	}

	void Shader::LoadFromFile(const Shader::ShaderType type, const tec::FilePath& fname) {
		auto _log = spdlog::get("console_log");
		if (!fname.isValidPath()) {
			_log->error("[Shader] Error loading shader: {} Invalid path: {}", fname.FileName(), fname.toString());
			return;
		}
		if (!fname.FileExists()) {
			_log->error("[Shader] Error loading shader: {} File don't exists. Check assets folder", fname.FileName());
			return;
		}
		std::ifstream fp(fname.GetNativePath(), std::ios_base::in);
		if (!fp.is_open()) {
			_log->error("[Shader] Error loading shader: {} File don't exists. Check open file.", fname.FileName());
			return;
		}
		std::string buffer(std::istreambuf_iterator<char>(fp), (std::istreambuf_iterator<char>()));
		LoadFromString(type, buffer, fname.FileName());
	}

	void Shader::LoadFromString(const Shader::ShaderType type, const std::string& source, const std::string& filename) {
		auto _log = spdlog::get("console_log");
		glGetError();
		GLuint shader = glCreateShader(type);
		if (auto error = glGetError() != GL_NO_ERROR) {
			_log->error("[Shader] Error creating shader : {} : {}", filename, error);
			return;
		}

		GLint length = static_cast<GLint>(source.length());
		const GLchar* str = source.data();
		glShaderSource(shader, 1, &str, &length);
		if (auto error = glGetError() != GL_NO_ERROR) {
			_log->error("[Shader] Error loading shader source : {} : {}", filename, error);
			return;
		}

		glCompileShader(shader);
		if (auto error = glGetError() != GL_NO_ERROR) {
			_log->error("[Shader] Error compiling shader : {} : {}", filename, error);
			return;
		}

		GLint status;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
		if (status == GL_FALSE) {
			GLint log_length;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
			GLchar* info_log = new GLchar[log_length];
			glGetShaderInfoLog(shader, log_length, NULL, info_log);
			_log->error("[Shader] Error compiling shader : {} : {}", filename, info_log);
			delete[] info_log;
		}

		if (auto error = glGetError() != GL_NO_ERROR) {
			_log->error("[Shader] Error compiling shader : {} : {}", filename, error);
			return;
		}
		this->shaders.push_back(shader);
	}

	void Shader::Build() {
		this->program = glCreateProgram();

		for (auto shader : this->shaders) {
			glAttachShader(this->program, shader);
		}

		glLinkProgram(this->program);

		GLint is_linked = 0;
		glGetProgramiv(this->program, GL_LINK_STATUS, &is_linked);
		if (is_linked == GL_FALSE) {
			GLint max_length = 0;
			glGetProgramiv(this->program, GL_INFO_LOG_LENGTH, &max_length);

			if (max_length > 0) {
				std::vector<GLchar> info_log(max_length);
				glGetProgramInfoLog(this->program, max_length, &max_length, &info_log[0]);
				std::string str(info_log.data());
				str += '\0';
				spdlog::get("console_log")->error("[Shader] Error linking : {}", str);
			}

			DeleteProgram();

			return;
		}

		for (auto shader : this->shaders) {
			glDetachShader(this->program, shader);
		}
	}

	void Shader::Use() {
		glUseProgram(this->program);
	}

	void Shader::UnUse() {
		glUseProgram(0);
	}

	void Shader::ActivateTextureUnit(const GLuint unit, const GLuint texture_name) {
		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_2D, texture_name);

	}

	void Shader::DeactivateTextureUnit(const GLuint unit) {
		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	GLint Shader::GetUniformLocation(const std::string name) {
		if (this->uniforms.find(name) != this->uniforms.end()) {
			return this->uniforms.at(name);
		}
		else {
			GLint uniform_id = glGetUniformLocation(this->program, name.c_str());
			if (uniform_id) {
				this->uniforms[name] = uniform_id;
			}
			return uniform_id;
		}
	}

	GLint Shader::GetAttributeLocation(const std::string name) {
		if (this->attributes.find(name) != this->attributes.end()) {
			return this->attributes.at(name);
		}
		else {
			GLint attribute_id = glGetAttribLocation(this->program, name.c_str());
			if (attribute_id) {
				this->attributes[name] = attribute_id;
			}
			return attribute_id;
		}
	}

	std::shared_ptr<Shader> Shader::CreateFromFile(const std::string name,
												   std::list<std::pair<Shader::ShaderType, FilePath>> filenames) {
		auto s = std::make_shared<Shader>();
		for (auto pair : filenames) {
			s->LoadFromFile(pair.first, pair.second);
		}
		s->Build();
		ShaderMap::Set(name, s);
		return s;
	}

	std::shared_ptr<Shader> Shader::CreateFromString(const std::string name,
													 std::list<std::pair<Shader::ShaderType, std::string>> source_code) {
		auto s = std::make_shared<Shader>();
		for (auto pair : source_code) {
			s->LoadFromString(pair.first, pair.second);
		}
		s->Build();
		ShaderMap::Set(name, s);
		return s;
	}
}
