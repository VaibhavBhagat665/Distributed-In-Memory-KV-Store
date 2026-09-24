#include "sharded_engine.h"
#include <future>

namespace kvstore {

ShardedEngine::ShardedEngine(size_t num_shards, size_t memory_limit_per_shard, EvictionPolicy policy)
    : num_shards_(num_shards), running_(false) {
    
    // Initialize shards (one HashTable per thread)
    shards_.reserve(num_shards_);
    for (size_t i = 0; i < num_shards_; ++i) {
        shards_.push_back(std::make_unique<HashTable>(16, memory_limit_per_shard, policy));
    }
    
    // Initialize request queues and synchronization primitives
    request_queues_.reserve(num_shards_);
    queue_mutexes_.reserve(num_shards_);
    queue_cvs_.reserve(num_shards_);
    worker_threads_.reserve(num_shards_);
    
    for (size_t i = 0; i < num_shards_; ++i) {
        request_queues_.push_back(std::make_unique<std::queue<std::unique_ptr<Request>>>());
        queue_mutexes_.push_back(std::make_unique<std::mutex>());
        queue_cvs_.push_back(std::make_unique<std::condition_variable>());
    }
}

ShardedEngine::~ShardedEngine() {
    stop();
}

void ShardedEngine::start() {
    if (running_) {
        return; // Already running
    }
    
    running_ = true;
    
    // Start worker threads
    for (size_t i = 0; i < num_shards_; ++i) {
        worker_threads_.push_back(
            std::make_unique<std::thread>(&ShardedEngine::worker_loop, this, i)
        );
    }
}

void ShardedEngine::stop() {
    if (!running_) {
        return; // Not running
    }
    
    running_ = false;
    
    // Wake up all worker threads
    for (auto& cv : queue_cvs_) {
        cv->notify_all();
    }
    
    // Join all threads
    for (auto& thread : worker_threads_) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }
    
    worker_threads_.clear();
}

size_t ShardedEngine::get_shard_id(const std::string& key) const {
    // Use the same hash function as HashTable for consistency
    uint64_t h = 0x9e3779b97f4a7c15ULL; // Golden ratio constant
    
    for (unsigned char c : key) {
        h ^= c;
        h *= 0x100000001b3ULL; // FNV prime
    }
    
    // Final mixing
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    
    return h % num_shards_;
}

void ShardedEngine::worker_loop(size_t shard_id) {
    while (running_) {
        std::unique_ptr<Request> req;
        
        {
            std::unique_lock<std::mutex> lock(*queue_mutexes_[shard_id]);
            
            // Wait for requests or shutdown signal
            queue_cvs_[shard_id]->wait(lock, [this, shard_id]() {
                return !request_queues_[shard_id]->empty() || !running_;
            });
            
            if (!running_ && request_queues_[shard_id]->empty()) {
                break; // Shutdown
            }
            
            if (!request_queues_[shard_id]->empty()) {
                req = std::move(request_queues_[shard_id]->front());
                request_queues_[shard_id]->pop();
            }
        }
        
        if (req) {
            process_request(shard_id, std::move(req));
        }
    }
}

void ShardedEngine::process_request(size_t shard_id, std::unique_ptr<Request> req) {
    HashTable& shard = *shards_[shard_id];
    
    switch (req->type) {
        case RequestType::GET: {
            auto result = shard.get(req->key);
            if (result.has_value()) {
                req->promise.set_value(*result.value());
            } else {
                req->promise.set_value(std::nullopt);
            }
            break;
        }
        
        case RequestType::SET: {
            bool success = shard.insert(req->key, req->value.value());
            // For SET, we don't return the value, just indicate success
            req->promise.set_value(std::nullopt);
            break;
        }
        
        case RequestType::REMOVE: {
            bool success = shard.remove(req->key);
            req->promise.set_value(std::nullopt);
            break;
        }
    }
}

std::optional<ValueEntry> ShardedEngine::get(const std::string& key) {
    size_t shard_id = get_shard_id(key);
    
    // Create request
    auto req = std::make_unique<Request>(RequestType::GET, key);
    auto future = req->promise.get_future();
    
    // Enqueue request
    {
        std::lock_guard<std::mutex> lock(*queue_mutexes_[shard_id]);
        request_queues_[shard_id]->push(std::move(req));
    }
    queue_cvs_[shard_id]->notify_one();
    
    // Wait for response
    return future.get();
}

bool ShardedEngine::set(const std::string& key, const ValueEntry& value) {
    size_t shard_id = get_shard_id(key);
    
    // Create request
    auto req = std::make_unique<Request>(RequestType::SET, key, value);
    auto future = req->promise.get_future();
    
    // Enqueue request
    {
        std::lock_guard<std::mutex> lock(*queue_mutexes_[shard_id]);
        request_queues_[shard_id]->push(std::move(req));
    }
    queue_cvs_[shard_id]->notify_one();
    
    // Wait for response
    future.get();
    return true;
}

bool ShardedEngine::remove(const std::string& key) {
    size_t shard_id = get_shard_id(key);
    
    // Create request
    auto req = std::make_unique<Request>(RequestType::REMOVE, key);
    auto future = req->promise.get_future();
    
    // Enqueue request
    {
        std::lock_guard<std::mutex> lock(*queue_mutexes_[shard_id]);
        request_queues_[shard_id]->push(std::move(req));
    }
    queue_cvs_[shard_id]->notify_one();
    
    // Wait for response
    future.get();
    return true;
}

size_t ShardedEngine::total_entries() const {
    size_t total = 0;
    for (const auto& shard : shards_) {
        total += shard->size();
    }
    return total;
}

} // namespace kvstore
