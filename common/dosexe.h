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

#ifndef COMMON_DOSEXE_H
#define COMMON_DOSEXE_H

#include "common/scummsys.h"

namespace Common {
	class SeekableReadStream;
	class SeekableWriteStream;

	struct MZHeader {
		uint16 	signature;
		uint16 	lastPageBytes;
		uint16	totalPages;
		uint16 	relocationCount;
		uint16	headerSize;
		uint16	minMemory;
		uint16	maxMemory;
		uint16	initSS;
		uint16	initSP;
		uint16	checksum;
		uint16	initIP;
		uint16	initCS;
		uint16	relocationTableOffset;
		uint16	overlayNumber;

		void read(SeekableReadStream *stream);
		void write(SeekableWriteStream *stream) const;
		bool valid() const;
	};

	struct MzExecutable {
		static SeekableReadStream *unpackLzExe(SeekableReadStream *src);
	};
}

#endif
