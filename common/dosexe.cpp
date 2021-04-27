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
#include "common/stream.h"

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

}
