#include "hotspine_writer.hpp"
#include "hotspine_layout.hpp"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <chrono>
#include <functional>
#include <sstream>
#include <iomanip>

namespace HotSpine {

// Simple symbol ID generator (in production, use a proper symbol mapping)
static uint32_t generateSymbolId(const std::string& exchange, 
                                const std::string& symbol,
                                const std::string& market_type) {
    std::stringstream ss;
    ss << exchange << ":" << symbol << ":" << market_type;
    std::string key = ss.str();
    
    // Simple hash function for symbol ID
    uint32_t hash = 5381;
    for (char c : key) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash;
}

HotSpineWriter::HotSpineWriter(const std::string& shm_name)
    : shm_name_(shm_name) {
    std::cout << "[HotSpineWriter] Initializing with shared memory name: " << shm_name << std::endl;
    if (!attachToSharedMemory()) {
        std::cerr << "[HotSpineWriter][ERROR] Failed to attach to shared memory: " << shm_name << std::endl;
    } else {
        std::cout << "[HotSpineWriter][INFO] Successfully initialized" << std::endl;
    }
}

HotSpineWriter::~HotSpineWriter() {
    std::cout << "[HotSpineWriter][INFO] Shutting down HotSpine writer" << std::endl;
    std::cout << "[HotSpineWriter][INFO] Final statistics: trades_written=" << trades_written_
              << ", write_errors=" << write_errors_ << std::endl;
    detachFromSharedMemory();
}

bool HotSpineWriter::attachToSharedMemory() {
    // Try to open existing shared memory, or create if it doesn't exist
    shm_fd_ = shm_open(shm_name_.c_str(), O_RDWR | O_CREAT, 0666);
    if (shm_fd_ == -1) {
        std::cerr << "HotSpineWriter: shm_open failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Try to get size of existing shared memory
    struct stat st;
    bool needs_init = false;
    if (fstat(shm_fd_, &st) == -1) {
        // Shared memory doesn't exist or can't be stat'd, we'll create it
        needs_init = true;
    } else if (st.st_size == 0) {
        // Shared memory exists but is empty, we'll initialize it
        needs_init = true;
    }
    
    size_t shm_size;
    if (needs_init) {
        // Calculate required size
        shm_size = HotSpine::calculateSharedMemorySize(HotSpine::DEFAULT_CAPACITY);
        
        // Set size of shared memory
        if (ftruncate(shm_fd_, shm_size) == -1) {
            std::cerr << "HotSpineWriter: ftruncate failed: " << strerror(errno) << std::endl;
            close(shm_fd_);
            shm_fd_ = -1;
            return false;
        }
    } else {
        shm_size = st.st_size;
    }
    
    // Map shared memory
    shm_ptr_ = mmap(nullptr, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
    if (shm_ptr_ == MAP_FAILED) {
        std::cerr << "HotSpineWriter: mmap failed: " << strerror(errno) << std::endl;
        close(shm_fd_);
        shm_fd_ = -1;
        return false;
    }
    
    // Initialize pointers
    header_ = static_cast<HotSpine::SharedMemoryHeader*>(shm_ptr_);
    trades_buffer_ = reinterpret_cast<HotSpine::HotTrade*>(static_cast<char*>(shm_ptr_) + sizeof(HotSpine::SharedMemoryHeader));
    
    // Initialize or validate header
    if (needs_init) {
        // Initialize header
        header_->version = HotSpine::HOTSPINE_VERSION;
        header_->capacity = HotSpine::DEFAULT_CAPACITY;
        header_->write_index = 0;
        header_->read_index = 0;
        header_->lost_count = 0;
        std::memset(header_->padding, 0, sizeof(header_->padding));
        
        std::cout << "HotSpineWriter: Created and initialized shared memory: " << shm_name_
                  << " (capacity: " << header_->capacity << " trades)" << std::endl;
    } else {
        // Validate header
        if (header_->version != HotSpine::HOTSPINE_VERSION) {
            std::cerr << "HotSpineWriter: Invalid shared memory version: " << header_->version
                      << " (expected: " << HotSpine::HOTSPINE_VERSION << ")" << std::endl;
            detachFromSharedMemory();
            return false;
        }
        
        std::cout << "HotSpineWriter: Successfully attached to shared memory: " << shm_name_
                  << " (capacity: " << header_->capacity << " trades)" << std::endl;
    }
    
    return true;
}

bool HotSpineWriter::detachFromSharedMemory() {
    if (shm_ptr_ != nullptr && shm_ptr_ != MAP_FAILED) {
        if (munmap(shm_ptr_, 0) == -1) {
            std::cerr << "HotSpineWriter: munmap failed: " << strerror(errno) << std::endl;
            return false;
        }
        shm_ptr_ = nullptr;
        header_ = nullptr;
        trades_buffer_ = nullptr;
    }
    
    if (shm_fd_ != -1) {
        close(shm_fd_);
        shm_fd_ = -1;
    }
    
    return true;
}

uint64_t HotSpineWriter::getCurrentTimestampMicros() {
    using namespace std::chrono;
    return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
}

uint32_t HotSpineWriter::getSymbolId(const std::string& exchange, 
                                     const std::string& symbol,
                                     const std::string& market_type) const {
    return generateSymbolId(exchange, symbol, market_type);
}

bool HotSpineWriter::isHealthy() const {
    if (shm_ptr_ == nullptr || header_ == nullptr) {
        std::cerr << "[HotSpineWriter][ERROR] isHealthy: Shared memory pointers are null" << std::endl;
        return false;
    }
      
    // Check if we're losing too many trades
    if (header_->lost_count > 1000) {
        std::cerr << "[HotSpineWriter][ERROR] isHealthy: Too many lost trades: " << header_->lost_count << std::endl;
        return false;
    }
      
    // Additional health checks
    if (header_->write_index >= header_->capacity) {
        std::cerr << "[HotSpineWriter][ERROR] isHealthy: Write index out of bounds: " << header_->write_index << "/" << header_->capacity << std::endl;
        return false;
    }
      
    if (header_->read_index >= header_->capacity) {
        std::cerr << "[HotSpineWriter][ERROR] isHealthy: Read index out of bounds: " << header_->read_index << "/" << header_->capacity << std::endl;
        return false;
    }
      
    // Calculate buffer utilization
    uint64_t used_slots = (header_->write_index >= header_->read_index)
        ? (header_->write_index - header_->read_index)
        : (header_->capacity - header_->read_index + header_->write_index);
    double utilization = (static_cast<double>(used_slots) / header_->capacity) * 100.0;
      
    std::cout << "[HotSpineWriter][INFO] isHealthy: OK (lost_count=" << header_->lost_count
              << ", write_index=" << header_->write_index
              << ", read_index=" << header_->read_index
              << ", capacity=" << header_->capacity
              << ", used_slots=" << used_slots
              << ", utilization=" << std::fixed << std::setprecision(2) << utilization << "%)" << std::endl;
      
    return true;
}

bool HotSpineWriter::writeTrade(const MarketData::Trade& trade) {
    std::cout << "[HotSpineWriter][DEBUG] writeTrade called for " << trade.exchange << ":" << trade.symbol
              << " @ " << trade.price << " x " << trade.quantity << std::endl;
    
    std::cout << "[HotSpineWriter][DEBUG] HotSpine Data Flow - Received trade: " << trade.exchange << ":" << trade.symbol
              << " @ " << trade.price << " x " << trade.quantity << " (" << trade.side << ")" << std::endl;
      
    if (!isHealthy()) {
        write_errors_++;
        std::cerr << "[HotSpineWriter][ERROR] writeTrade failed: writer not healthy" << std::endl;
        std::cerr << "[HotSpineWriter][ERROR] HotSpine Data Flow - Writer health check failed" << std::endl;
        return false;
    }
     
    // Use batching if enabled
    if (batching_enabled_) {
        std::cout << "[HotSpineWriter][DEBUG] Adding trade to batch (batch_size=" << batch_buffer_.size() + 1 << "/" << batch_size_ << ")" << std::endl;
        std::lock_guard<std::mutex> lock(batch_mutex_);
        batch_buffer_.push_back(trade);
         
        // Flush batch if it reaches the batch size
        if (batch_buffer_.size() >= batch_size_) {
            std::cout << "[HotSpineWriter][DEBUG] Batch full, flushing..." << std::endl;
            flushBatch();
        }
         
        trades_written_++;
        std::cout << "[HotSpineWriter][DEBUG] Trade added to batch successfully" << std::endl;
        return true;
    }
    
    // Direct write (non-batched mode)
    // Convert trade to HotTrade format
    HotSpine::HotTrade hot_trade;
    hot_trade.ts_exchange = static_cast<uint64_t>(trade.timestamp_us);
    hot_trade.ts_local = getCurrentTimestampMicros();
    hot_trade.price = trade.price;
    hot_trade.size = trade.quantity;
    hot_trade.symbol_id = getSymbolId(trade.exchange, trade.symbol, trade.market_type);
    hot_trade.side = (trade.side == "buy") ? 0 : 1;
    
    // Calculate write position
    uint64_t write_index = header_->write_index;
    uint64_t next_write_index = (write_index + 1) % header_->capacity;
    
    // Check if buffer is full
    std::cout << "[HotSpineWriter][DEBUG] Direct write: checking buffer space (write_index=" << write_index
              << ", next_write_index=" << next_write_index
              << ", read_index=" << header_->read_index
              << ", capacity=" << header_->capacity << ")" << std::endl;
     
    if (next_write_index == header_->read_index) {
        // Buffer is full, increment lost count
        std::cerr << "[HotSpineWriter][ERROR] Direct write: Buffer is full!" << std::endl;
        header_->lost_count++;
        write_errors_++;
        return false;
    }
     
    // Write trade to buffer
    std::cout << "[HotSpineWriter][DEBUG] Direct write: Writing trade for " << trade.exchange << ":" << trade.symbol
              << " @ " << trade.price << " x " << trade.quantity
              << " to buffer index " << write_index << std::endl;
     
    trades_buffer_[write_index] = hot_trade;
     
    std::cout << "[HotSpineWriter][DEBUG] Direct write: Updating write index from " << header_->write_index
              << " to " << next_write_index << std::endl;
    
    // Update write index (memory barrier ensured by atomic operations)
    header_->write_index = next_write_index;
     
    trades_written_++;
    std::cout << "[HotSpineWriter][INFO] Direct write: Successfully wrote trade. Total written: " << trades_written_ << std::endl;
    std::cout << "[HotSpineWriter][INFO] HotSpine Data Flow - Trade successfully processed and written to shared memory" << std::endl;
    return true;
}

void HotSpineWriter::flushBatch() {
    std::cout << "[HotSpineWriter][DEBUG] flushBatch called (batch_size=" << batch_buffer_.size() << ")" << std::endl;
     
    if (!isHealthy()) {
        std::cerr << "[HotSpineWriter][ERROR] flushBatch: writer not healthy" << std::endl;
        return;
    }
     
    if (batch_buffer_.empty()) {
        std::cout << "[HotSpineWriter][DEBUG] flushBatch: batch is empty" << std::endl;
        return;
    }
     
    std::vector<MarketData::Trade> batch_to_write;
    {
        std::lock_guard<std::mutex> lock(batch_mutex_);
        batch_to_write.swap(batch_buffer_);
    }
     
    std::cout << "[HotSpineWriter][DEBUG] flushBatch: processing " << batch_to_write.size() << " trades" << std::endl;
     
    if (batch_to_write.empty()) {
        std::cout << "[HotSpineWriter][DEBUG] flushBatch: batch_to_write is empty after swap" << std::endl;
        return;
    }
    
    // Reserve space in shared memory
    uint64_t current_write_index = header_->write_index;
    uint64_t required_space = batch_to_write.size();
    
    // Check if there's enough space
    uint64_t available_space = (header_->read_index > current_write_index)
        ? (header_->read_index - current_write_index - 1)
        : (header_->capacity - current_write_index + header_->read_index - 1);
     
    std::cout << "[HotSpineWriter][DEBUG] flushBatch: space check (required=" << required_space
              << ", available=" << available_space
              << ", write_index=" << current_write_index
              << ", read_index=" << header_->read_index
              << ", capacity=" << header_->capacity << ")" << std::endl;
    
    if (available_space < required_space) {
        // Not enough space, increment lost count
        std::cerr << "[HotSpineWriter][ERROR] flushBatch: Not enough space! Required: " << required_space
                  << ", Available: " << available_space << std::endl;
        header_->lost_count += batch_to_write.size();
        write_errors_ += batch_to_write.size();
        return;
    }
    
    // Write batch to shared memory
    std::cout << "[HotSpineWriter][DEBUG] flushBatch: Writing " << batch_to_write.size()
              << " trades to shared memory starting at index " << current_write_index << std::endl;
     
    for (size_t i = 0; i < batch_to_write.size(); i++) {
        const auto& trade = batch_to_write[i];
        HotSpine::HotTrade hot_trade;
        hot_trade.ts_exchange = static_cast<uint64_t>(trade.timestamp_us);
        hot_trade.ts_local = getCurrentTimestampMicros();
        hot_trade.price = trade.price;
        hot_trade.size = trade.quantity;
        hot_trade.symbol_id = getSymbolId(trade.exchange, trade.symbol, trade.market_type);
        hot_trade.side = (trade.side == "buy") ? 0 : 1;
        
        std::cout << "[HotSpineWriter][DEBUG] Writing trade " << i+1 << "/" << batch_to_write.size()
                  << " for " << trade.exchange << ":" << trade.symbol
                  << " @ " << trade.price << " x " << trade.quantity
                  << " to buffer index " << current_write_index << std::endl;
         
        trades_buffer_[current_write_index] = hot_trade;
        current_write_index = (current_write_index + 1) % header_->capacity;
    }
     
    std::cout << "[HotSpineWriter][DEBUG] flushBatch: Updating write index from "
              << header_->write_index << " to " << current_write_index << std::endl;
    
    // Update write index (memory barrier ensured by atomic operations)
    header_->write_index = current_write_index;
     
    trades_written_ += batch_to_write.size();
    std::cout << "[HotSpineWriter][INFO] flushBatch: Successfully wrote " << batch_to_write.size()
              << " trades. Total written: " << trades_written_ << std::endl;
}

bool HotSpineWriter::writeTrades(const std::vector<MarketData::Trade>& trades) {
    std::cout << "[HotSpineWriter][DEBUG] writeTrades called with " << trades.size() << " trades" << std::endl;
    if (!isHealthy()) {
        std::cerr << "[HotSpineWriter][ERROR] writeTrades: Writer not healthy" << std::endl;
        write_errors_++;
        return false;
    }
     
    bool all_success = true;
    for (const auto& trade : trades) {
        if (!writeTrade(trade)) {
            all_success = false;
        }
    }
     
    std::cout << "[HotSpineWriter][INFO] writeTrades completed: " << trades.size()
              << " trades processed, " << (all_success ? "all successful" : "some failed") << std::endl;
     
    return all_success;
}
 
std::string HotSpineWriter::getDetailedStats() const {
    if (!isHealthy()) {
        return "{\"error\": \"HotSpine writer not healthy\"}";
    }
     
    std::stringstream ss;
    ss << "{"
       << "\"trades_written\": " << trades_written_ << ", "
       << "\"write_errors\": " << write_errors_ << ", "
       << "\"lost_count\": " << header_->lost_count << ", "
       << "\"write_index\": " << header_->write_index << ", "
       << "\"read_index\": " << header_->read_index << ", "
       << "\"capacity\": " << header_->capacity << ", "
       << "\"version\": " << header_->version;
     
    // Calculate buffer utilization
    uint64_t used_slots = (header_->write_index >= header_->read_index)
        ? (header_->write_index - header_->read_index)
        : (header_->capacity - header_->read_index + header_->write_index);
    double utilization = (static_cast<double>(used_slots) / header_->capacity) * 100.0;
     
    ss << ", \"used_slots\": " << used_slots
       << ", \"utilization_percent\": " << std::fixed << std::setprecision(2) << utilization
       << ", \"batching_enabled\": " << (batching_enabled_ ? "true" : "false")
       << ", \"batch_size\": " << batch_size_
       << ", \"batch_buffer_size\": " << batch_buffer_.size()
       << "}";
     
    return ss.str();
}
 
} // namespace HotSpine