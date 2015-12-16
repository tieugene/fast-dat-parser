#include <array>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

#include "hash.hpp"

typedef std::array<uint8_t, 32> hash_t;

struct Block {
	hash_t hash;
	hash_t prevBlockHash;
	uint32_t bits;

	Block () {}
	Block (const hash_t& hash, const hash_t& prevBlockHash, const uint32_t bits) : hash(hash), prevBlockHash(prevBlockHash), bits(bits) {}
};

struct Chain {
	Block block;
	Chain* previous = nullptr;
	size_t work = 0;

	Chain () {}
	Chain (Block block, Chain* previous) : block(block), previous(previous) {}

	size_t determineAggregateWork () {
		if (this->work != 0) return this->work;
		this->work = this->block.bits;

		if (this->previous != nullptr) {
			this->work += this->previous->determineAggregateWork();
		}

		return this->work;
	}

	template <typename F>
	void forEach (const F f) const {
		auto link = this;

		while (link != nullptr) {
			f(*link);

			link = (*link).previous;
		}
	}
};

auto& findChains(std::map<hash_t, Chain>& chains, const std::map<hash_t, Block>& blockMap, const Block& root) {
	const auto blockChainIter = chains.find(root.hash);

	// already built this?
	if (blockChainIter != chains.end()) return blockChainIter->second;

	// not yet built, what about the previous block?
	const auto prevBlockIter = blockMap.find(root.prevBlockHash);

	// if prevBlock is unknown, it must be a genesis block
	if (prevBlockIter == blockMap.end()) {
		chains[root.hash] = Chain(root, nullptr);

		return chains[root.hash];
	}

	// otherwise, recurse down to the genesis block, finding the chain on the way back
	const auto prevBlock = prevBlockIter->second;
	auto& prevBlockChain = findChains(chains, blockMap, prevBlock);

	chains[root.hash] = Chain(root, &prevBlockChain);

	return chains[root.hash];
}

// find all chains who are not parents to any other blocks (aka, a chain tip)
auto findChainTips(std::map<hash_t, Chain>& chains) {
	std::map<hash_t, bool> parents;

	for (auto& chainIter : chains) {
		auto&& chain = chainIter.second;
		if (chain.previous == nullptr) continue;

		parents[chain.previous->block.hash] = true;
	}

	std::vector<Chain> tips;
	for (auto& chainIter : chains) {
		auto&& chain = chainIter.second;
		if (parents.find(chain.block.hash) != parents.end()) continue;

		tips.push_back(chain);
	}

	return tips;
}

auto findBest(std::vector<Chain> chains) {
	auto bestChain = *chains.begin();
	size_t mostWork = 0;

	for (auto&& chain : chains) {
		auto work = chain.determineAggregateWork();

		if (work > mostWork) {
			bestChain = chain;
			mostWork = work;
		}
	}

	std::vector<Block> blockchain;
	bestChain.forEach([&](const Chain& chain) {
		blockchain.push_back(chain.block);
	});

	return blockchain;
}

int main () {
	std::vector<Block> blocks;

	do {
		uint8_t buffer[80];
		const auto read = fread(&buffer[0], sizeof(buffer), 1, stdin);

		// EOF?
		if (read == 0) break;

		hash_t hash, prevBlockHash;
		uint32_t bits;

		hash256(&hash[0], &buffer[0], 80);
		memcpy(&prevBlockHash[0], &buffer[4], 32);
		memcpy(&bits, &buffer[72], 4);

		blocks.push_back(Block(hash, prevBlockHash, bits));
	} while (true);

	// build a hash map for easy referencing
	std::map<hash_t, Block> blockMap;
	for (auto& block : blocks) {
		blockMap[block.hash] = block;
	}

	// find all possible chains
	std::map<hash_t, Chain> chains;
	for (auto& block : blocks) {
		findChains(chains, blockMap, block);
	}

	const auto chainTips = findChainTips(chains);
	std::cerr << "Found " << chainTips.size() << " chain tips" << std::endl;

	// now find the best
	const auto bestBlockChain = findBest(chainTips);
	const auto genesis = bestBlockChain.back();
	const auto tip = bestBlockChain.front();

	// output
	std::cerr << "Found best chain" << std::endl;
	std::cerr << "- Height: " << bestBlockChain.size() - 1 << std::endl;
	std::cerr << std::hex;
	std::cerr << "- Genesis: ";
	for (size_t i = 31; i < 32; --i) std::cerr << std::setw(2) << std::setfill('0') << (uint32_t) genesis.hash[i];
	std::cerr << std::endl << "- Tip: ";
	for (size_t i = 31; i < 32; --i) std::cerr << std::setw(2) << std::setfill('0') << (uint32_t) tip.hash[i];
	std::cerr << std::endl << std::dec;

	for(auto it = bestBlockChain.rbegin(); it != bestBlockChain.rend(); ++it) {
		fwrite(&it->hash[0], 32, 1, stdout);
	}

	return 0;
}
