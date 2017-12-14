/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "WarpPeerCapability.h"
#include "SnapshotStorage.h"

#include "libethcore/Exceptions.h"

namespace dev
{
namespace eth
{

WarpPeerCapability::WarpPeerCapability(std::shared_ptr<p2p::SessionFace> _s, p2p::HostCapabilityFace* _h, unsigned _i, p2p::CapDesc const& /* _cap */):
	Capability(_s, _h, _i)
{
}

void WarpPeerCapability::init(unsigned _hostProtocolVersion, u256 _hostNetworkId, u256 _chainTotalDifficulty, h256 _chainCurrentHash, h256 _chainGenesisHash, std::shared_ptr<SnapshotStorageFace const> _snapshot)
{
	m_snapshot = _snapshot;
	
	h256 snapshotBlockHash;
	u256 snapshotBlockNumber;
	if (m_snapshot)
	{
		bytes const snapshotManifest(m_snapshot->readManifest());
		RLP manifest(snapshotManifest);
		if (manifest.itemCount() != 6)
			BOOST_THROW_EXCEPTION(InvalidSnapshotManifest());
		snapshotBlockNumber = manifest[4].toInt<u256>(RLP::VeryStrict);
		snapshotBlockHash = manifest[5].toHash<h256>(RLP::VeryStrict);
	}

	requestStatus(_hostProtocolVersion, _hostNetworkId, _chainTotalDifficulty, _chainCurrentHash, _chainGenesisHash, snapshotBlockHash, snapshotBlockNumber);
}

bool WarpPeerCapability::interpret(unsigned _id, RLP const& _r)
{
	if (!m_snapshot)
		return false;

	try
	{
		switch (_id)
		{
		case WarpStatusPacket:
		{
			// TODO check that packet has enough elements

			// Packet layout:
			// [ version:P, state_hashes : [hash_1:B_32, hash_2 : B_32, ...],  block_hashes : [hash_1:B_32, hash_2 : B_32, ...],
			//		state_root : B_32, block_number : P, block_hash : B_32 ]
			auto const protocolVersion = _r[0].toInt<unsigned>();
			auto const networkId = _r[1].toInt<u256>();
			auto const totalDifficulty = _r[2].toInt<u256>();
			auto const latestHash = _r[3].toHash<h256>();
			auto const genesisHash = _r[4].toHash<h256>();
			auto const snapshotHash = _r[5].toHash<h256>();
			auto const snapshotNumber = _r[6].toInt<u256>();

			clog(p2p::NetMessageSummary) << "Status: "
				<< "protocol version " << protocolVersion
				<< "networkId " << networkId
				<< "genesis hash " << genesisHash
				<< "total difficulty " << totalDifficulty
				<< "latest hash" << latestHash
				<< "snapshot hash" << snapshotHash
				<< "snapshot number" << snapshotNumber;
			break;
		}
		case GetSnapshotManifest:
		{
			RLPStream s;
			prep(s, SnapshotManifest, 1).appendRaw(m_snapshot->readManifest());
			sealAndSend(s);
			break;
		}
		case GetSnapshotData:
		{
			const h256 chunkHash = _r[0].toHash<h256>(RLP::VeryStrict);

			RLPStream s;
			prep(s, SnapshotManifest, 1).append(m_snapshot->readCompressedChunk(chunkHash));
			sealAndSend(s);
			break;
		}
		case GetBlockHeadersPacket:
		{
			// TODO We are being asked DAO for block sometimes, need to be able to answer this
 			/* const auto blockHash = _r[0].toHash<h256>();
			const auto blockNumber = _r[0].toInt<bigint>();
			const auto maxHeaders = _r[1].toInt<u256>();
			const auto skip = _r[2].toInt<u256>();
			const auto reverse = _r[3].toInt<bool>();*/

			RLPStream s;
			prep(s, BlockHeadersPacket);
			sealAndSend(s);
			break;
		}
		default:
			return false;
		}
	}
	catch (Exception const&)
	{
		clog(p2p::NetWarn) << "Warp Peer causing an Exception:" << boost::current_exception_diagnostic_information() << _r;
	}
	catch (std::exception const& _e)
	{
		clog(p2p::NetWarn) << "Warp Peer causing an exception:" << _e.what() << _r;
	}

	return true;
}

void WarpPeerCapability::requestStatus(unsigned _hostProtocolVersion, u256 const& _hostNetworkId, u256 const& _chainTotalDifficulty, h256 const& _chainCurrentHash, h256 const& _chainGenesisHash, 
	h256 const& _snapshotBlockHash, u256 const& _snapshotBlockNumber)
{
	RLPStream s;
	prep(s, WarpStatusPacket, 7)
		<< _hostProtocolVersion
		<< _hostNetworkId
		<< _chainTotalDifficulty
		<< _chainCurrentHash
		<< _chainGenesisHash
		<< _snapshotBlockHash
		<< _snapshotBlockNumber;
	sealAndSend(s);
}

}
}
