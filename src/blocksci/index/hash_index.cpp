//
//  hash_index.cpp
//  blocksci
//
//  Created by Harry Kalodner on 8/27/17.
//
//

#include "hash_index.hpp"
#include <blocksci/util/bitcoin_uint256.hpp>
#include <blocksci/scripts/bitcoin_base58.hpp>
#include <blocksci/scripts/script_info.hpp>
#include <blocksci/address/address.hpp>
#include <blocksci/util/util.hpp>

#include <array>

namespace blocksci {
    
    HashIndex::HashIndex(const std::string &path, bool readonly) {
        rocksdb::Options options;
        // Optimize RocksDB. This is the easiest way to get RocksDB to perform well
        options.IncreaseParallelism();
        options.OptimizeLevelStyleCompaction();
        // create the DB if it's not already present
        options.create_if_missing = true;
        options.create_missing_column_families = true;
        
        
        std::vector<rocksdb::ColumnFamilyDescriptor> columnDescriptors;
        blocksci::for_each(blocksci::AddressInfoList(), [&](auto tag) {
            auto options = rocksdb::ColumnFamilyOptions{};
            auto descriptor = rocksdb::ColumnFamilyDescriptor{addressName(tag), options};
            columnDescriptors.push_back(descriptor);
        });
        columnDescriptors.emplace_back(rocksdb::kDefaultColumnFamilyName, rocksdb::ColumnFamilyOptions{});
        columnDescriptors.emplace_back("T", rocksdb::ColumnFamilyOptions{});
        
        rocksdb::DB *dbPtr;
        std::vector<rocksdb::ColumnFamilyHandle *> columnHandlePtrs;
        if (readonly) {
            rocksdb::Status s = rocksdb::DB::OpenForReadOnly(options, path.c_str(), columnDescriptors, &columnHandlePtrs, &dbPtr);
            assert(s.ok());
        } else {
            rocksdb::Status s = rocksdb::DB::Open(options, path.c_str(), columnDescriptors, &columnHandlePtrs, &dbPtr);
        }
        db = std::unique_ptr<rocksdb::DB>(dbPtr);
        for (auto handle : columnHandlePtrs) {
            columnHandles.emplace_back(std::unique_ptr<rocksdb::ColumnFamilyHandle>(handle));
        }
    }
    
    std::unique_ptr<rocksdb::ColumnFamilyHandle> &HashIndex::getColumn(AddressType::Enum type) {
        auto index = static_cast<size_t>(type);
        return columnHandles.at(index);
    }
    
    std::unique_ptr<rocksdb::ColumnFamilyHandle> &HashIndex::getColumn(DedupAddressType::Enum type) {
        switch (type) {
            case DedupAddressType::PUBKEY:
                return getColumn(AddressType::PUBKEYHASH);
            case DedupAddressType::SCRIPTHASH:
                return getColumn(AddressType::SCRIPTHASH);
            case DedupAddressType::MULTISIG:
                return getColumn(AddressType::MULTISIG);
            case DedupAddressType::NULL_DATA:
                return getColumn(AddressType::NULL_DATA);
            case DedupAddressType::NONSTANDARD:
                return getColumn(AddressType::NONSTANDARD);
        }
        assert(false);
        return getColumn(AddressType::NONSTANDARD);
    }
    
    void HashIndex::addTx(const uint256 &hash, uint32_t txNum) {
        rocksdb::Slice keySlice(reinterpret_cast<const char *>(&hash), sizeof(hash));
        rocksdb::Slice valueSlice(reinterpret_cast<const char *>(&txNum), sizeof(txNum));
        db->Put(rocksdb::WriteOptions(), columnHandles.back().get(), keySlice, valueSlice);
    }
    
    uint32_t HashIndex::getTxIndex(const uint256 &txHash) {
        return getMatch(columnHandles.back().get(), txHash);
    }
    
    uint32_t HashIndex::getPubkeyHashIndex(const uint160 &pubkeyhash) {
        return getMatch(getColumn(AddressType::PUBKEYHASH).get(), pubkeyhash);
    }
    
    uint32_t HashIndex::getScriptHashIndex(const uint160 &scripthash) {
        return getMatch(getColumn(AddressType::SCRIPTHASH).get(), scripthash);
    }
    
    uint32_t HashIndex::getScriptHashIndex(const uint256 &scripthash) {
        return getMatch(getColumn(AddressType::WITNESS_SCRIPTHASH).get(), scripthash);
    }
}
