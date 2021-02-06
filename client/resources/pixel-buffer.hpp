#pragma once

#include <mutex>
#include <memory>
#include <cstdint>
#include <string>

#include "multiton.hpp"
#include "filesystem.hpp"

namespace tec {
	class PixelBuffer;
	typedef Multiton<std::string, std::shared_ptr<PixelBuffer>> PixelBufferMap;

	enum class ImageColorMode : std::uint32_t {
		UNKNOWN_MODE = 0xffffffffu,
		MONOCHROME = 0,
		MONOCHROME_A = 4,
		COLOR_RGB = 2,
		COLOR_RGBA = 6,
	};

	class PixelBuffer final {
	public:
		PixelBuffer() = default;
		/**
		 * \brief Returns a resource with the specified name.
		 *
		 * \param[in] const FilePath filename The filename of the image file to load.
		 * \return bool True if initialization finished with no errors.
		 */
		bool Load(const FilePath& filename);

		PixelBuffer(const PixelBuffer&) = delete;
		PixelBuffer& operator=(const PixelBuffer&) = delete;

		PixelBuffer(PixelBuffer&&) noexcept;
		PixelBuffer& operator=(PixelBuffer&&) noexcept;

		/** \brief Construct a new pixel buffer.
		 * \param width the width of the buffer in pixels
		 * \param height the height of the buffer in pixels
		 * \param bitspersample the number of bits in a single channel, usually 8
		 * \param mode greyscale (Monochrome) or color (RGB), and whether there is alpha or no alpha
		 */
		PixelBuffer(std::uint32_t width, std::uint32_t height, std::uint32_t bitspersample, ImageColorMode mode);

		/** \brief Create a copy of a pixel buffer.
		 * \param pbuf the pixel buffer to copy from
		 * \return true on success
		 */
		bool CreateCopy(const PixelBuffer& pbuf);

		/** \brief Create a copy of a pixel buffer in a different format.
		 * \param pbuf the pixel buffer to copy from
		 * \param bitspersample the number of bits in a single channel, usually 8
		 * \param mode greyscale (Monochrome) or color (RGB), and whether there is alpha or no alpha
		 * \return true on success
		 */
		bool CreateCopy(const PixelBuffer& pbuf, std::uint32_t bitspersample, ImageColorMode mode);

		/** \brief Create a new pixel buffer, replacing the old buffer (if any).
		 * \param width the width of the buffer in pixels
		 * \param height the height of the buffer in pixels
		 * \param bitspersample the number of bits in a single channel, usually 8
		 * \param mode greyscale (Monochrome) or color (RGB), and whether there is alpha or no alpha
		 * \return true on success
		 */
		bool Create(uint32_t width, uint32_t height, uint32_t bitspersample, ImageColorMode mode);

		/**
		 * \brief Factory method that creates a PixelBuffer and stores it in the
		 * PixelBufferMap under name.  It will optionally load a texture file
		 * with the given filename.
		 * \param const std::string name The name to store the PixelBuffer under.
		 * \param const FilePath filename The optional filename of the image to load.
		 * \return std::shared_ptr<PixelBuffer> The created PixelBuffer.
		 */
		static std::shared_ptr<PixelBuffer> Create(const std::string& name, const FilePath& filename = FilePath());

		bool IsDirty() const;
		/** \brief Mark dirty */
		void Invalidate();
		/** \brief Mark not dirty */
		void Validate();

		/**
		 * \brief Returns a pointer to the base address of image data for reading.
		 * This function is intended for loading pixel data from the buffer.
		 * \return uint8_t * base image address or nullptr if invalid.
		 */
		const std::uint8_t* GetBlockBase() const;

		std::mutex& GetWritelock() { return this->writelock; }
		std::uint8_t* GetPtr() { return this->blockptr.get(); }

		std::uint32_t Width() const { return imagewidth > 0 ? static_cast<std::uint32_t>(imagewidth) : 0 ; }
		std::uint32_t Height() const { return imageheight > 0 ? static_cast<std::uint32_t>(imageheight) : 0 ; }
		std::uint32_t Pitch() const { return bufferpitch; }
		std::uint32_t PixelSize() const { return imagepixelsize; }
		ImageColorMode GetFormat() const { return imagemode; }

		// output a PPM image to stderr or file as a debug feature
		void PPMDebug();
		void PPMDebug(const char*);

		// meta data, used by some formats such as cursors
		std::uint32_t image_x{ 0 };
		std::uint32_t image_y{ 0 };

	protected:
		std::int32_t imagewidth{ 0 };
		std::int32_t imageheight{ 0 };

		/// number of bytes to move vertical 1 raster line
		std::uint32_t bufferpitch{ 0 };
		std::uint32_t imagepixelsize{ 0 };
		std::uint32_t imagebitdepth{ 0 };
		ImageColorMode imagemode{ ImageColorMode::MONOCHROME };

		bool dirty{ false };

		std::unique_ptr<std::uint8_t[]> blockptr;
		std::mutex writelock;
	};

}
