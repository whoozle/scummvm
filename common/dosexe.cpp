/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "common/dosexe.h"
#include "common/array.h"
#include "common/debug.h"
#include "common/memstream.h"
#include "common/stream.h"
#include "common/substream.h"
#include "common/textconsole.h"

namespace Common {

	void MZHeader::read(SeekableReadStream *stream) {
		signature = stream->readUint16LE();
		lastPageBytes = stream->readUint16LE();
		totalPages = stream->readUint16LE();
		relocationCount = stream->readUint16LE();
		headerSize = stream->readUint16LE();
		minMemory = stream->readUint16LE();
		maxMemory = stream->readUint16LE();
		initSS = stream->readUint16LE();
		initSP = stream->readUint16LE();
		checksum = stream->readUint16LE();
		initIP = stream->readUint16LE();
		initCS = stream->readUint16LE();
		relocationTableOffset = stream->readUint16LE();
		overlayNumber = stream->readUint16LE();
	}

	void MZHeader::write(SeekableWriteStream *stream) const {
		stream->writeUint16LE(signature);
		stream->writeUint16LE(lastPageBytes);
		stream->writeUint16LE(totalPages);
		stream->writeUint16LE(relocationCount);
		stream->writeUint16LE(headerSize);
		stream->writeUint16LE(minMemory);
		stream->writeUint16LE(maxMemory);
		stream->writeUint16LE(initSS);
		stream->writeUint16LE(initSP);
		stream->writeUint16LE(checksum);
		stream->writeUint16LE(initIP);
		stream->writeUint16LE(initCS);
		stream->writeUint16LE(relocationTableOffset);
		stream->writeUint16LE(overlayNumber);
	}

	bool MZHeader::valid() const {
		return signature == 0x5a4d;
	}


	SeekableReadStream *MzExecutable::unpackLzExe(SeekableReadStream *src) {
		src->seek(0);
		MZHeader header;
		header.read(src);
		if (!header.valid()) {
			warning("invalid header signature");
			return nullptr;
		}
		auto loaderOffset = header.initCS * 0x10 + header.headerSize * 0x10;
		debug("LZEXE: loader offset: %08x", loaderOffset);
		src->seek(loaderOffset);
		auto exeIP = src->readUint16LE();
		auto exeCS = src->readUint16LE();
		auto exeSP = src->readUint16LE();
		auto exeSS = src->readUint16LE();
		debug("LZEXE: entry point: %04x:%04x, stack: %04x:%04x", exeCS, exeIP, exeSS, exeSP);
		SeekableSubReadStream packedData(src, 0x20, loaderOffset);
		debug("LZEXE: compressed data size: %u", packedData.size());

		Common::Array<uint8> unpackedData;
		uint bitCount = 16;
		uint16 bits = packedData.readUint16LE();
		auto readBit = [&bits, &bitCount, &packedData]() {
			uint16 next = bits & 1;
			bits >>= 1;
			--bitCount;
			if (bitCount == 0) {
				bits = packedData.readUint16LE();
				bitCount = 16;
			}
			return next;
		};

		while(true) {
			auto bit = readBit();
			if (bit) {
				auto value = packedData.readByte();
				unpackedData.push_back(value);
			} else {
				bit = readBit();
				uint16 len;
				int offset;
				if (!bit) {
					len = readBit() << 1;
					len |= readBit();
					len += 2;
					offset = packedData.readByte();
					offset -= 0x100;
				} else {
					uint16 word = packedData.readUint16LE();
					len = (word >> 8) & 7;
					offset = (((word >> 11) << 8) | (word & 0xff)) - 0x2000;
					if (len) {
						len += 2;
					} else {
						auto next = packedData.readByte();
						if (next == 0) {
							break;
						}
						if (next != 1)
							len = 1 + next;
					}
				}
				while(len--) {
					unpackedData.push_back(unpackedData[unpackedData.size() + offset]);
				}
			}
		}
		Common::hexdump(unpackedData.data(), unpackedData.size());

		SeekableSubReadStream packedRelocs(src, loaderOffset + 0x158, src->size());
		debug("LZEXE: compressed relocations size: %u", packedRelocs.size());

		Common::Array<uint> relocs;
		uint32 offset = 0;
		while(true) {
			uint16 delta = packedRelocs.readByte();
			if (delta == 0) {
				delta = packedRelocs.readUint16LE();
				if (delta == 0) {
					offset += 0xfff0;
					offset &= 0xffff0;
					continue;
				} else if (delta == 1) {
					break;
				}
			}
			offset += delta;
			relocs.push_back(offset);
		}

		return nullptr;
	}
}
