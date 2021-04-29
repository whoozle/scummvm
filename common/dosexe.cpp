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

	void MZHeader::updateSize() {
		headerSize = relocationCount * 4 + 0x1c;
		headerSize = (headerSize + 15) / 16;
	}

	SeekableReadStream *MzExecutable::unpackLzExe(SeekableReadStream *src) {
		src->seek(0);
		MZHeader header;
		header.read(src);
		if (!header.valid()) {
			warning("invalid header signature");
			return nullptr;
		}
		debug("LZEXE: packed exe mem min: 0x%06x, max: 0x%06x", header.minMemory * 0x10, header.maxMemory * 0x10);
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

		header.relocationCount = relocs.size();
		header.updateSize();

		auto fileSize = header.headerSize * 16 + unpackedData.size();
		debug("LZEXE: unpacked file size: %u, %+d", fileSize, unpackedData.size() - packedData.size());

		header.totalPages = (fileSize + 0x1ff) / 0x200;
		header.lastPageBytes = fileSize & 0x1ff;
		header.minMemory = (header.minMemory + unpackedData.size() - packedData.size() + 15) / 16;
		header.checksum = 0;
		header.overlayNumber = 0;
		header.relocationTableOffset = 0x1c;

		header.initCS = exeCS;
		header.initIP = exeIP;
		header.initSS = exeSS;
		header.initSP = exeSP;

		auto unpackedExe = new Common::MemoryWriteStreamDynamic(DisposeAfterUse::NO);

		header.write(unpackedExe);
		for(auto reloc : relocs) {
			unpackedExe->writeUint16LE(reloc & 0x0f);
			unpackedExe->writeUint16LE(reloc >> 4);
		}
		unsigned padding = header.headerSize * 16 - unpackedExe->size();
		while(padding--)
			unpackedExe->writeByte(0);

		unpackedExe->write(unpackedData.data(), unpackedData.size());

		return new Common::MemoryReadStream(unpackedExe->getData(), unpackedExe->size(), DisposeAfterUse::YES);
	}

	bool MzExecutable::load(SeekableReadStream *src)
	{
		debug("LOADING EXE");
		MZHeader header;
		src->seek(0);
		header.read(src);
		if (!header.valid()) {
			warning("invalid MZ header");
			return false;
		}

		src->seek(header.relocationTableOffset);
		Common::Array<uint32> relocations;
		relocations.reserve(header.relocationCount);
		for(uint16 i = 0; i < header.relocationCount; ++i) {
			uint32 offset = src->readUint16LE();
			offset += static_cast<uint32>(src->readUint16LE()) << 4;
			debug("relocation at %06x", offset);
			relocations.push_back(offset);
		}

		Common::Array<bool> segmentPresent(0x10000, false);
		segmentPresent[0] = true;

		auto exeStart = header.headerSize * 16;
		auto exeSize = src->size() - exeStart;
		for(auto & reloc : relocations) {
			src->seek(exeStart + reloc);
			auto seg = src->readUint16LE();
			segmentPresent[seg] = true;
		}

		for(uint i = 0; i < segmentPresent.size(); ++i) {
			if (segmentPresent[i])
				segments.push_back(i);
		}

		for(uint i = 0; i < segments.size(); ++i) {
			auto segment = segments[i];
			int32 begin = segment << 4;
			if (begin >= exeSize)
				continue;

			int32 end = i + 1 < segments.size()? segments[i + 1] << 4: exeSize;
			if (end > exeSize)
				end = exeSize;

			debug("segment: %06x - %06x, size: %d", begin, end, end - begin);
			src->seek(exeStart + begin);
			auto & data = segmentData[segment];
			data.resize(end - begin);
			src->read(data.data(), data.size());
		}

		return true;
	}
}
